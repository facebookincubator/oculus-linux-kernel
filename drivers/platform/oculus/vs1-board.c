#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/ktime.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/thermal.h>
#include <linux/version.h>

#define VS1_GPIO_ALLOC 0x8000000UL

#define is_configured(_gpio) ((_gpio)&VS1_GPIO_ALLOC)
#define to_gpio(_gpio) ((_gpio) & ~VS1_GPIO_ALLOC)

#define gpio_safe_release(_gpio)			\
	do {						\
		if (is_configured((_gpio))) {		\
			gpio_free(to_gpio((_gpio)));	\
			(_gpio) = 0;			\
		}					\
	} while (0)

#define GPIO_INPUT 0
#define GPIO_OUTPUT 1

#define NUM_CAM_PWDN_GPIOS 4

#define DEFAULT_PWM_PERIOD (40 * 1000)

#define MAX_PWM 255
#define FAN_FAILURE_TIME_MS 3000

#define MAX_NUM_THERMAL_ZONES 29

#define FAN_STATE_DEFAULT 1
#define FAN_STATE_TRIP 2

struct fan_ctx {
	struct device *dev;
	struct mutex lock;
	struct workqueue_struct *fan_critical_wq;
	struct delayed_work fan_critical_poll;
	unsigned int tz_trips[MAX_NUM_THERMAL_ZONES];
	unsigned int tz_hysteresis[MAX_NUM_THERMAL_ZONES];
	bool tripped_zones[MAX_NUM_THERMAL_ZONES];
	struct thermal_zone_device tz_devices[MAX_NUM_THERMAL_ZONES];
	size_t num_thermal_zones;
	/* variables below are protected by the mutex */
	struct pwm_device *pwm;
	struct thermal_cooling_device *cdev;
	bool is_screen_on;
	unsigned char pwm_value;
	unsigned char last_reported_pwm;

	unsigned int fan_state;
	unsigned int fan_max_state;
	unsigned int *fan_cooling_levels;

	unsigned int power;
	struct regulator *vdd;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_suspend;

	ktime_t time_stamp;
	ktime_t fan_pre_t;
	ktime_t fan_diff_t;
	unsigned int irq;
	u64 rpm_count;
	u64 irq_rpm_count;
	bool force_failure;

	struct notifier_block fb_notif;
};

struct vs1_ctx {
	struct device *dev;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
};

struct vs1_gpio {
	const char *name;
	int out_value;
	unsigned int *target;
};

struct vs1_irq {
	const char *name;
	int out_value;
	unsigned int *target;
};

static int32_t get_pin_control(struct device *dev, struct pinctrl **ppinctrl,
		struct pinctrl_state **active,
		struct pinctrl_state **inactive,
		const char *active_name,
		const char *inactive_name)
{
	int32_t rc = 0;
	struct pinctrl *pinctrl = NULL;

	if (!dev || !ppinctrl || !active || !inactive || !active_name ||
			!inactive_name)
		return -EINVAL;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		rc = PTR_ERR_OR_ZERO(pinctrl) ?: -EINVAL;
		dev_err(dev, "failed to look up pinctrl");
		return rc;
	}

	*active = pinctrl_lookup_state(pinctrl, active_name);
	if (IS_ERR_OR_NULL(*active)) {
		rc = PTR_ERR_OR_ZERO(*active) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		return rc;
	}

	*inactive = pinctrl_lookup_state(pinctrl, inactive_name);
	if (IS_ERR_OR_NULL(*inactive)) {
		rc = PTR_ERR_OR_ZERO(*inactive) ?: -EINVAL;
		dev_err(dev, "falied to look up suspend pin state");
		return rc;
	}

	*ppinctrl = pinctrl;

	return 0;
}


/*
 * VS1 fan platform driver
 */
static int fan_power_locked(struct fan_ctx *ctx, bool on)
{
	int rc = 0;

	if (pwm_is_enabled(ctx->pwm) == on)
		return 0;

	if (on) {
		if (ctx->vdd != NULL) {
			rc = regulator_enable(ctx->vdd);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to enable fan regulator: %d",
						__func__, rc);
				return rc;
			}
		} else if (is_configured(ctx->power)) {
			rc = gpio_direction_output(to_gpio(ctx->power), 1);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to enable fan GPIO: %d",
						__func__, rc);
				return rc;
			}
		} else {
			dev_err(ctx->dev, "%s: fan not configured", __func__);
			return -ENODEV;
		}

		rc = pwm_enable(ctx->pwm);
		if (rc < 0) {
			dev_err(ctx->dev, "%s: failed to enable fan PWM: %d",
					__func__, rc);
			return rc;
		}
	} else {
		pwm_disable(ctx->pwm);

		if (ctx->vdd != NULL) {
			rc = regulator_disable(ctx->vdd);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to disable fan regulator: %d",
						__func__, rc);
				return rc;
			}
		} else if (is_configured(ctx->power)) {
			rc = gpio_direction_output(to_gpio(ctx->power), 0);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to disable fan GPIO: %d",
						__func__, rc);
				return rc;
			}
		} else {
			dev_err(ctx->dev, "%s: fan not configured", __func__);
			return -ENODEV;
		}
	}

	return 0;
}

static int fan_set_pwm_locked(struct fan_ctx *ctx, unsigned long pwm)
{
	unsigned long duty = 0;
	int rc = 0;

	if (pwm == ctx->pwm_value)
		return 0;

	duty = DIV_ROUND_UP(pwm * (ctx->pwm->period - 1), MAX_PWM);
	rc = pwm_config(ctx->pwm, duty, ctx->pwm->period);
	if (rc < 0) {
		dev_err(ctx->dev, "%s: failed to set fan pwm_config: %d",
				__func__, rc);
		return rc;
	}

	ctx->pwm_value = pwm;

	/* toggle power when pwm is 0 or non-zero */
	fan_power_locked(ctx, !!ctx->pwm_value);

	return 0;
}

static ssize_t set_force_failure(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long force_failure;

	if (kstrtoul(buf, 10, &force_failure))
		return -EINVAL;

	ctx->force_failure = (force_failure != 0);

	return count;
}

static ssize_t show_force_failure(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", ctx->force_failure);
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long pwm = 0;
	ssize_t rc = 0;

	if (kstrtoul(buf, 10, &pwm) || pwm > MAX_PWM)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ctx->last_reported_pwm = pwm;
	if (ctx->is_screen_on)
		rc = fan_set_pwm_locked(ctx, pwm);
	mutex_unlock(&ctx->lock);
	if (rc)
		return -rc;

	return count;
}

static ssize_t show_pwm(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);
	s64 elapsed_ms = ktime_to_ms(ktime_sub(ktime_get(), ctx->time_stamp));

	if (elapsed_ms > 1000)
		ctx->rpm_count = 0;

	return scnprintf(buf, PAGE_SIZE, "RPM: %llu\n", ctx->rpm_count / 2);
}
static DEVICE_ATTR(force_failure, 0600, show_force_failure, set_force_failure);
static DEVICE_ATTR(pwm1, S_IRUGO | S_IWUSR, show_pwm, set_pwm);

static struct attribute *fan_attrs[] = {
	&dev_attr_force_failure.attr,
	&dev_attr_pwm1.attr, NULL,
};
ATTRIBUTE_GROUPS(fan);


static int fan_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct fan_ctx *ctx = cdev->devdata;

	if (!ctx)
		return -ENODEV;

	*state = ctx->fan_max_state;

	return 0;
}

static bool fan_has_failure(struct fan_ctx *ctx)
{
	s64 elapsed_ms;
	unsigned long pwm;

	if (ctx->force_failure)
		return true;

	mutex_lock(&ctx->lock);
	elapsed_ms = ktime_to_ms(ktime_sub(ktime_get(), ctx->time_stamp));
	pwm = ctx->pwm_value;
	mutex_unlock(&ctx->lock);

	return ((pwm != 0) && (elapsed_ms > FAN_FAILURE_TIME_MS));
}

static int fan_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct fan_ctx *ctx = cdev->devdata;

	if (!ctx)
		return -ENODEV;

	if (!ctx->is_screen_on)
		*state = 0;
	else if (fan_has_failure(ctx))
		*state = ctx->fan_max_state + 1;
	else
		*state = ctx->fan_state;

	return 0;
}

static int
fan_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct fan_ctx *ctx = cdev->devdata;
	int ret;

	if (!ctx)
		return -ENODEV;

	if (state > ctx->fan_max_state)
		return -EINVAL;

	if (ctx->is_screen_on == 0)
		state = 0;

	if (state == ctx->fan_state && ctx->pwm_value == ctx->fan_cooling_levels[state])
		return 0;

	mutex_lock(&ctx->lock);

	ret = fan_set_pwm_locked(ctx, ctx->fan_cooling_levels[state]);
	if (ret) {
		dev_err(&cdev->device, "Cannot set pwm!\n");
		goto fan_set_state_unlock;
	}
	ctx->fan_state = state;

fan_set_state_unlock:
	mutex_unlock(&ctx->lock);
	return ret;
}

static const struct thermal_cooling_device_ops fan_cooling_ops = {
	.get_max_state = fan_get_max_state,
	.get_cur_state = fan_get_cur_state,
	.set_cur_state = fan_set_cur_state,
};

static void fan_poll_tz(struct work_struct *work)
{
	struct fan_ctx *ctx = container_of(work,
		struct fan_ctx, fan_critical_poll.work);
	int fan_state = FAN_STATE_DEFAULT;
	int cur_temp;
	int i = 0;

	/* Find all of the thermal zones that will trip the fan */
	for (i = 0; i < ctx->num_thermal_zones; i++) {
		if (!ctx->tz_devices[i].ops->get_temp)
			continue;

		ctx->tz_devices[i].ops->get_temp(&ctx->tz_devices[i], &cur_temp);

		if (cur_temp >= ctx->tz_trips[i]) {
			ctx->tripped_zones[i] = true;
			/* Set fan state */
			fan_state = FAN_STATE_TRIP;
		} else if (ctx->tripped_zones[i] &&
			cur_temp <= (ctx->tz_trips[i] - ctx->tz_hysteresis[i]))
			ctx->tripped_zones[i] = false;
	}

	fan_set_cur_state(ctx->cdev, fan_state);

	/* Reschedule */
	schedule_delayed_work(&ctx->fan_critical_poll,
			msecs_to_jiffies(1000));
}

static irqreturn_t fan_irq_handler(int irq, void *dev)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);
	ktime_t zero = ktime_set(0, 0);

	ctx->time_stamp = ktime_get();

	/* Get seconds */
	if (ktime_compare(ctx->fan_pre_t, zero))
		ctx->fan_diff_t = ktime_add(ktime_sub(ctx->time_stamp,
					ctx->fan_pre_t), ctx->fan_diff_t);

	ctx->fan_pre_t = ctx->time_stamp;

	ctx->irq_rpm_count++;

	if (ktime_to_ms(ctx->fan_diff_t) >= 1000) {
		ctx->rpm_count = ctx->irq_rpm_count * 60;
		ctx->fan_diff_t = ktime_set(0, 0);
		ctx->fan_pre_t = ktime_set(0, 0);
		ctx->irq_rpm_count = 0;
	}

	return IRQ_HANDLED;
}

static int fan_configure_irq(struct device *dev, struct vs1_irq *table)
{
	struct device_node *of_node = dev->of_node;
	struct vs1_irq *vi;
	int gpio_num, irq_num, rc;

	for (vi = table; vi->name != NULL; vi++) {
		gpio_num = rc = of_get_named_gpio(of_node, vi->name, 0);
		if (rc < 0) {
			dev_err(dev, "failed to get irq '%s': %d",
					vi->name, rc);
			return rc;
		}

		rc = devm_gpio_request(dev, gpio_num, "fan_irq_gpio");
		if (rc < 0) {
			dev_err(dev, "failed to request GPIO %d", gpio_num);
			return rc;
		}

		gpio_direction_input(gpio_num);

		irq_num = rc = gpio_to_irq(gpio_num);
		if (rc < 0) {
			dev_err(dev, "failed to get '%s': %d", vi->name, rc);
			return rc;
		}

		rc = devm_request_irq(dev, irq_num, (irq_handler_t)fan_irq_handler,
				IRQF_TRIGGER_FALLING, "fan_irq", dev);
		if (rc) {
			dev_err(dev, "failed to get '%s': %d", vi->name, rc);
			return rc;
		}

		rc = enable_irq_wake(gpio_num);
		if (rc) {
			dev_err(dev, "failed to register fan irq '%s': %d",
					vi->name, rc);
			return rc;
		}
	}

	return 0;
}

static int fan_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct fan_ctx *ctx = platform_get_drvdata(pdev);
	int rc;

	mutex_lock(&ctx->lock);

	rc = fan_power_locked(ctx, false);
	if (rc)
		dev_err(ctx->dev, "fan_power error");

	mutex_unlock(&ctx->lock);

	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_suspend);
	if (rc)
		dev_err(ctx->dev, "pinctrl_select_state error");

	return rc;
}

static int fan_resume(struct platform_device *pdev)
{
	struct fan_ctx *ctx = platform_get_drvdata(pdev);
	int rc;

	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_default);
	if (rc)
		dev_err(ctx->dev, "pinctrl_select_state error");

	mutex_lock(&ctx->lock);

	rc = fan_power_locked(ctx, true);
	if (rc)
		dev_err(ctx->dev, "fan_power error");

	mutex_unlock(&ctx->lock);

	return rc;
}

/*
 * respond to screen status in fan driver
 */
static int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fan_ctx *ctx = container_of(self, struct fan_ctx, fb_notif);
	struct fb_event *evdata = data;
	int *blank;

	/* If we aren't interested in this event, skip it immediately ... */
	if (event != FB_EVENT_BLANK)
		return 0;

	if (evdata && evdata->data && ctx && ctx->dev) {
		blank = evdata->data;
		mutex_lock(&ctx->lock);
		if (*blank == FB_BLANK_UNBLANK) {
			ctx->is_screen_on = true;
			fan_set_pwm_locked(ctx, ctx->last_reported_pwm);
		} else if (*blank == FB_BLANK_POWERDOWN ||
		    *blank == FB_BLANK_VSYNC_SUSPEND) {
			ctx->is_screen_on = false;
			fan_set_pwm_locked(ctx, 0);
		}
		mutex_unlock(&ctx->lock);
	}
	return 0;
}

static int fan_of_get_cooling_data(struct device *dev,
				       struct fan_ctx *ctx)
{
	struct device_node *np = dev->of_node;
	int num, i, ret;

	if (!of_find_property(np, "cooling-levels", NULL))
		return 0;

	ret = of_property_count_u32_elems(np, "cooling-levels");
	if (ret <= 0) {
		dev_err(dev, "Invalid cooling-levels property!\n");
		return ret ? : -EINVAL;
	}

	num = ret;
	ctx->fan_cooling_levels = devm_kcalloc(dev, num, sizeof(u32),
						   GFP_KERNEL);
	if (!ctx->fan_cooling_levels)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "cooling-levels",
					 ctx->fan_cooling_levels, num);
	if (ret) {
		dev_err(dev, "Property 'cooling-levels' cannot be read!\n");
		return ret;
	}

	for (i = 0; i < num; i++) {
		if (ctx->fan_cooling_levels[i] > MAX_PWM) {
			dev_err(dev, "VS1 fan state[%d]:%d > %d\n", i,
				ctx->fan_cooling_levels[i], MAX_PWM);
			return -EINVAL;
		}
	}

	ctx->fan_max_state = num - 1;

	return 0;
}

static int fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_cooling_device *cdev;
	struct device *hwmon;
	struct device_node *of_node = dev->of_node;
	struct fan_ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	struct vs1_irq fan_irq_table[] = {
		{"fan-irq", 1, &ctx->irq},
		{},
	};
	uint32_t voltage[2] = {0};
	int gpio_num = 0;
	struct thermal_zone_device *tzd;
	int tz_count = 0;
	int tz_trip_count = 0;
	int tz_hysteresis_count = 0;
	int i;
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	ctx->fan_critical_wq = alloc_workqueue("fan_critical_wq",
		WQ_HIGHPRI, 0);

	if (!ctx->fan_critical_wq) {
		rc = -ENOMEM;
		return rc;
	}

	mutex_init(&ctx->lock);

	rc = fan_configure_irq(dev, fan_irq_table);
	if (rc) {
		dev_err(dev, "Failed to configure fan IRQ, rc: %d\n", rc);
		return rc;
	}

	rc = get_pin_control(dev, &ctx->pinctrl, &ctx->pin_default,
			&ctx->pin_suspend, "fan_default", "fan_suspend");
	if (rc) {
		dev_err(dev, "Failed to get pin control, rc: %d\n", rc);
		return rc;
	}

	ctx->pwm = devm_of_pwm_get(&pdev->dev, pdev->dev.of_node, NULL);
	if (IS_ERR(ctx->pwm)) {
		rc = PTR_ERR(ctx->pinctrl);
		dev_err(&pdev->dev, "Could not get PWM\n");
		return rc;
	}

	ctx->pwm->period = DEFAULT_PWM_PERIOD;
	ctx->pwm_value = 0;
	ctx->is_screen_on = false;
	ctx->last_reported_pwm = 0;

	hwmon = devm_hwmon_device_register_with_groups(&pdev->dev, "oculus",
			ctx, fan_groups);
	if (IS_ERR(hwmon)) {
		rc = PTR_ERR(hwmon);
		dev_err(&pdev->dev, "Failed to register hwmon device\n");
		pwm_disable(ctx->pwm);
		return rc;
	}

	ctx->fb_notif.notifier_call = fb_notifier_callback;
	rc = fb_register_client(&ctx->fb_notif);
	if (rc) {
		dev_err(dev, "%s: Failed to register fb_notifier: %d",
					__func__, rc);
		return rc;
	}

	gpio_num = of_get_named_gpio(of_node, "fan-power", 0);
	if (gpio_is_valid(gpio_num)) {
		/* Fan power GPIO for proto1 */
		rc = gpio_request(gpio_num, "fan-power");
		if (rc < 0) {
			dev_err(dev, "%s: failed to request fan power GPIO: %d",
					__func__, rc);
			return rc;
		}
		ctx->power = (unsigned)gpio_num | VS1_GPIO_ALLOC;
	} else {
		/* Regulator power for proto2 */
		ctx->vdd = devm_regulator_get(dev, "fan,vdd");
		if (IS_ERR(ctx->vdd)) {
			rc = PTR_ERR(ctx->vdd);
			dev_err(dev, "%s: failed to get fan power regulator: %d",
					__func__, rc);
			return rc;
		}

		rc = of_property_read_u32_array(of_node,
				"fan,vdd-voltage-level", voltage, 2);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to read fan regulator voltage levels: %d",
					__func__, rc);
			return rc;
		}

		rc = regulator_set_voltage(ctx->vdd, voltage[0], voltage[1]);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to set fan regulator voltage: %d",
					__func__, rc);
			return rc;
		}

		/* 600mA */
		rc = regulator_set_load(ctx->vdd, 600000);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to set fan regulator load: %d",
					__func__, rc);
			return rc;
		}
	}

	platform_set_drvdata(pdev, ctx);

	rc = fan_of_get_cooling_data(&pdev->dev, ctx);
	if (rc) {
		dev_err(&pdev->dev, "Failed to get fan cooling data");
		return rc;
	}

	if (IS_ENABLED(CONFIG_THERMAL)) {
		cdev = thermal_of_cooling_device_register(pdev->dev.of_node,
			"oculus,fan", ctx, &fan_cooling_ops);
		if (IS_ERR(cdev)) {
			rc = PTR_ERR(cdev);
			dev_err(&pdev->dev,
				"Failed to register oculus,fan as cooling device");
			return rc;
		}
		ctx->cdev = cdev;
		thermal_cdev_update(cdev);
	}

	tz_count = of_property_count_strings(of_node, "thermal-zones");
	ctx->num_thermal_zones = tz_count;

	tz_trip_count = of_property_count_u32_elems(of_node, "thermal-zone-trips");
	if (tz_trip_count != tz_count) {
		dev_err(dev, "Invalid number of thermal trips defined");
		return -EINVAL;
	}

	tz_hysteresis_count = of_property_count_u32_elems(of_node, "thermal-zone-hysteresis");
	if (tz_hysteresis_count != tz_count) {
		dev_err(dev, "Invalid number of thermal hysteresis values defined");
		return -EINVAL;
	}

	for (i = 0; i < ctx->num_thermal_zones; i++) {
		const char *tz_name = NULL;

		int err = of_property_read_string_index(of_node, "thermal-zones", i,
			&tz_name);

		if (err) {
			dev_err(dev, "Unable to read thermal zone name at index: %d", i);
			continue;
		}

		tzd = thermal_zone_get_zone_by_name(tz_name);
		if (IS_ERR(tzd)) {
			dev_err(dev, "Error getting thermal zone %s", tz_name);
			continue;
		}

		ctx->tz_devices[i] = *tzd;
	}

	rc = of_property_read_u32_array(of_node, "thermal-zone-trips", ctx->tz_trips, tz_trip_count);
	if (rc) {
		dev_err(dev, "Unable to read thermal zone trip points");
		return rc;
	}

	rc = of_property_read_u32_array(of_node, "thermal-zone-hysteresis",
									ctx->tz_hysteresis, tz_hysteresis_count);
	if (rc) {
		dev_err(dev, "Unable to read thermal zone hysteresis values");
		return rc;
	}

	INIT_DEFERRABLE_WORK(&ctx->fan_critical_poll, fan_poll_tz);
	schedule_delayed_work(&ctx->fan_critical_poll,
		msecs_to_jiffies(1000));

	return 0;
}

static int fan_remove(struct platform_device *pdev)
{
	struct fan_ctx *ctx = platform_get_drvdata(pdev);

	mutex_lock(&ctx->lock);
	fan_power_locked(ctx, false);
	mutex_unlock(&ctx->lock);

	thermal_cooling_device_unregister(ctx->cdev);

	if (is_configured(ctx->power))
		gpio_safe_release(ctx->power);

	if (!IS_ERR(ctx->vdd)) {
		regulator_disable(ctx->vdd);
		regulator_put(ctx->vdd);
	}

	if (ctx->fan_critical_wq)
		destroy_workqueue(ctx->fan_critical_wq);

	fb_unregister_client(&ctx->fb_notif);

	return 0;
}

static const struct of_device_id fan_of_match[] = {
	{
		.compatible = "oculus,fan",
	},
	{},
};

static struct platform_driver fan_driver = {
	.driver = {
		.name = "oculus,fan", .of_match_table = fan_of_match,
	},
	.probe = fan_probe,
	.remove = fan_remove,
#ifdef CONFIG_PM
	.suspend = fan_suspend,
	.resume = fan_resume,
#endif
};

static int vs1_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vs1_ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	dev_info(dev, "start vs1 board configuration\n");

	ctx->dev = dev;

	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl)) {
		rc = PTR_ERR_OR_ZERO(ctx->pinctrl) ?: -EINVAL;
		dev_err(dev, "failed to look up pinctrl");
		return rc;
	}

	ctx->pin_default = pinctrl_lookup_state(ctx->pinctrl, "vs1_defaults");
	if (IS_ERR_OR_NULL(ctx->pin_default)) {
		rc = PTR_ERR_OR_ZERO(ctx->pin_default) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		return rc;
	}

	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_default);
	if (rc) {
		dev_err(ctx->dev, "pinctrl_select_state error");
		return rc;
	}

	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "vs1 board configured\n");

	return rc;
}

static int vs1_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id vs1_of_match[] = {
	{
		.compatible = "oculus,vs1",
	},
	{},
};

static struct platform_driver vs1_driver = {
	.driver = {
		.name = "oculus_vs1", .of_match_table = vs1_of_match,
	},
	.probe = vs1_probe,
	.remove = vs1_remove,
};

static int __init vs1_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&vs1_driver);
	if (rc) {
		pr_err("Unable to register vs1 platform driver: %d\n", rc);
		return rc;
	}

	rc = platform_driver_register(&fan_driver);
	if (rc) {
		pr_err("Unable to register fan platform driver: %d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit vs1_driver_exit(void)
{
	platform_driver_unregister(&vs1_driver);
	platform_driver_unregister(&fan_driver);
}

module_init(vs1_driver_init);
module_exit(vs1_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for VS1 board");
