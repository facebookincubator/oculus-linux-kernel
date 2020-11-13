#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regulator/consumer.h>
#include <linux/time.h>
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

struct fan_ctx {
	struct device *dev;
	struct mutex lock;
	/* variables below are protected by the mutex */
	struct pwm_device *pwm;
	bool is_screen_on;
	unsigned char pwm_value;
	unsigned char last_reported_pwm;

	unsigned int power;
	struct regulator *vdd;

	struct pinctrl *pinctrl;
	struct pinctrl_state *pin_default;
	struct pinctrl_state *pin_suspend;

	struct timeval time_stamp;
	unsigned int irq;
	unsigned int rpm_count;

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
		goto error;
	}

	*active = pinctrl_lookup_state(pinctrl, active_name);
	if (IS_ERR_OR_NULL(*active)) {
		rc = PTR_ERR_OR_ZERO(*active) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		goto free_pinctrl;
	}

	*inactive = pinctrl_lookup_state(pinctrl, inactive_name);
	if (IS_ERR_OR_NULL(*inactive)) {
		rc = PTR_ERR_OR_ZERO(*inactive) ?: -EINVAL;
		dev_err(dev, "falied to look up suspend pin state");
		goto free_pinctrl;
	}

	*ppinctrl = pinctrl;

	return rc;

free_pinctrl:
	devm_pinctrl_put(pinctrl);
error:
	return rc;
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
				goto fan_power_exit;
			}
		} else if (is_configured(ctx->power)) {
			rc = gpio_direction_output(to_gpio(ctx->power), 1);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to enable fan GPIO: %d",
						__func__, rc);
				goto fan_power_exit;
			}
		} else {
			dev_err(ctx->dev, "%s: fan not configured", __func__);
			goto fan_power_exit;
		}

		rc = pwm_enable(ctx->pwm);
		if (rc < 0) {
			dev_err(ctx->dev, "%s: failed to enable fan PWM: %d",
					__func__, rc);
			goto fan_power_exit;
		}
	} else {

		pwm_disable(ctx->pwm);

		if (ctx->vdd != NULL) {
			rc = regulator_disable(ctx->vdd);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to disable fan regulator: %d",
						__func__, rc);
				goto fan_power_exit;
			}
		} else if (is_configured(ctx->power)) {
			rc = gpio_direction_output(to_gpio(ctx->power), 0);
			if (rc < 0) {
				dev_err(ctx->dev, "%s: failed to disable fan GPIO: %d",
						__func__, rc);
				goto fan_power_exit;
			}
		} else {
			dev_err(ctx->dev, "%s: fan not configured", __func__);
			goto fan_power_exit;
		}
	}

fan_power_exit:
	return rc;
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
	struct timeval current_t;
	int rpm_count = 0;

	do_gettimeofday(&current_t);

	if ((current_t.tv_sec - ctx->time_stamp.tv_sec) > 1)
		ctx->rpm_count = 0;

	if (ctx->rpm_count > 0)
		rpm_count = ctx->rpm_count/2;
	else if (ctx->pwm_value > 0 && ctx->rpm_count == 0)
		rpm_count = -1;

	return scnprintf(buf, PAGE_SIZE, "RPM: %d\n", rpm_count);
}
static DEVICE_ATTR(pwm1, S_IRUGO | S_IWUSR, show_pwm, set_pwm);

static struct attribute *fan_attrs[] = {
	&dev_attr_pwm1.attr, NULL,
};
ATTRIBUTE_GROUPS(fan);

static irqreturn_t fan_irq_handler(int irq, void *dev)
{
	struct fan_ctx *ctx = dev_get_drvdata(dev);
	static time_t pre_t;
	static time_t diff_t;
	static unsigned long rpm_count;

	do_gettimeofday(&ctx->time_stamp);

	/* Get seconds */
	if (pre_t)
		diff_t = diff_t + (ctx->time_stamp.tv_sec - pre_t);

	pre_t = ctx->time_stamp.tv_sec;
	rpm_count++;

	if (diff_t >= 1) {
		ctx->rpm_count = rpm_count * 60;
		diff_t = pre_t = 0;
		rpm_count = 0;
	}

	return IRQ_HANDLED;
}

static int fan_configure_irq(struct device *dev, struct vs1_irq *table)
{
	struct device_node *of_node = dev->of_node;
	struct vs1_irq *vi;
	int rc = 0;

	for (vi = table; vi->name != NULL; vi++) {
		int gpio_num;

		gpio_num = rc = of_get_named_gpio(of_node, vi->name, 0);
		if (rc < 0) {
			dev_err(dev, "failed to get irq '%s': %d",
					vi->name, rc);
			goto fail;
		}

		gpio_request(gpio_num, "fan_irq_gpio");

		gpio_direction_input(gpio_num);

		rc = gpio_to_irq(gpio_num);
		if (rc < 0) {
			dev_err(dev, "failed to get '%s': %d", vi->name, rc);
			goto fail;
		}

		rc = request_irq(rc, (irq_handler_t)fan_irq_handler,
				IRQF_TRIGGER_FALLING, "fan_irq", dev);
		if (rc) {
			dev_err(dev, "failed to get '%s': %d", vi->name, rc);
			gpio_free(gpio_num);
			goto fail;
		}

		rc = enable_irq_wake(gpio_num);
		if (rc)
			dev_err(dev, "failed to register fan irq '%s': %d",
					vi->name, rc);
	}

	return 0;

fail:
	for (vi = table; vi->name != NULL; vi++)
		gpio_safe_release(*vi->target);

	return rc;
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

static int fan_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device *hwmon;
	struct device_node *of_node = dev->of_node;
	struct fan_ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	struct vs1_irq fan_irq_table[] = {
		{"fan-irq", 1, &ctx->irq},
		{},
	};
	uint32_t voltage[2] = {0};
	int gpio_num = 0;
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	mutex_init(&ctx->lock);

	rc = fan_configure_irq(dev, fan_irq_table);
	if (rc)
		goto fan_fail_alloc;

	rc = get_pin_control(dev, &ctx->pinctrl, &ctx->pin_default,
			&ctx->pin_suspend, "fan_default", "fan_suspend");
	if (rc) {
		dev_err(dev, "Failed to get pin control, rc: %d\n", rc);
		goto fan_fail_alloc;
	}

	ctx->pwm = devm_of_pwm_get(&pdev->dev, pdev->dev.of_node, NULL);
	if (IS_ERR(ctx->pwm)) {
		dev_err(&pdev->dev, "Could not get PWM\n");
		goto fan_fail_pinctrl;
	}

	ctx->pwm->period = DEFAULT_PWM_PERIOD;
	ctx->pwm_value = 0;
	ctx->is_screen_on = false;
	ctx->last_reported_pwm = 0;

	hwmon = devm_hwmon_device_register_with_groups(&pdev->dev, "oculus",
			ctx, fan_groups);
	if (IS_ERR(hwmon)) {
		dev_err(&pdev->dev, "Failed to register hwmon device\n");
		pwm_disable(ctx->pwm);
		goto fan_fail_pwm;
	}

	ctx->fb_notif.notifier_call = fb_notifier_callback;
	rc = fb_register_client(&ctx->fb_notif);
	if (rc) {
		dev_err(dev, "%s: Failed to register fb_notifier: %d",
					__func__, rc);
		goto fan_fail_pwm;
	}

	gpio_num = of_get_named_gpio(of_node, "fan-power", 0);
	if (gpio_is_valid(gpio_num)) {
		/* Fan power GPIO for proto1 */
		rc = gpio_request(gpio_num, "fan-power");
		if (rc < 0) {
			dev_err(dev, "%s: failed to request fan power GPIO: %d",
					__func__, rc);
			goto fan_fail_gpio;
		}
		ctx->power = (unsigned)gpio_num | VS1_GPIO_ALLOC;
	} else {
		/* Regulator power for proto2 */
		ctx->vdd = regulator_get(dev, "fan,vdd");
		if (IS_ERR(ctx->vdd)) {
			dev_err(dev, "%s: failed to get fan power regulator: %d",
					__func__, rc);
			goto fan_fail_pwm;
		}

		rc = of_property_read_u32_array(of_node,
				"fan,vdd-voltage-level", voltage, 2);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to read fan regulator voltage levels: %d",
					__func__, rc);
			goto fan_fail_reg;
		}

		rc = regulator_set_voltage(ctx->vdd, voltage[0], voltage[1]);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to set fan regulator voltage: %d",
					__func__, rc);
			goto fan_fail_reg;
		}

		/* 600mA */
		rc = regulator_set_load(ctx->vdd, 600000);
		if (rc < 0) {
			dev_err(dev, "%s: Unable to set fan regulator load: %d",
					__func__, rc);
			goto fan_fail_reg;
		}
	}

	platform_set_drvdata(pdev, ctx);

	return 0;

fan_fail_gpio:
	ctx->power = -EINVAL;
	goto fan_fail_pwm;

fan_fail_reg:
	regulator_put(ctx->vdd);
	ctx->vdd = NULL;
fan_fail_pwm:
	devm_pwm_put(&pdev->dev, ctx->pwm);
fan_fail_pinctrl:
	devm_pinctrl_put(ctx->pinctrl);
fan_fail_alloc:
	devm_kfree(dev, ctx);
	return rc;
}

static int fan_remove(struct platform_device *pdev)
{
	struct fan_ctx *ctx = platform_get_drvdata(pdev);

	mutex_lock(&ctx->lock);
	fan_power_locked(ctx, false);
	mutex_unlock(&ctx->lock);

	if (is_configured(ctx->power))
		gpio_safe_release(ctx->power);

	if (!IS_ERR(ctx->vdd)) {
		regulator_disable(ctx->vdd);
		regulator_put(ctx->vdd);
	}

	devm_pwm_put(&pdev->dev, ctx->pwm);
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
		goto fail_pinctrl;
	}

	ctx->pin_default = pinctrl_lookup_state(ctx->pinctrl, "vs1_defaults");
	if (IS_ERR_OR_NULL(ctx->pin_default)) {
		rc = PTR_ERR_OR_ZERO(ctx->pin_default) ?: -EINVAL;
		dev_err(dev, "failed to look up default pin state");
		goto free_pinctrl;
	}

	rc = pinctrl_select_state(ctx->pinctrl, ctx->pin_default);
	if (rc) {
		dev_err(ctx->dev, "pinctrl_select_state error");
		goto free_pinctrl;
	}

	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "vs1 board configured\n");

	return rc;

free_pinctrl:
	devm_pinctrl_put(ctx->pinctrl);
fail_pinctrl:
	devm_kfree(dev, ctx);
	return rc;
}

static int vs1_remove(struct platform_device *pdev)
{
	struct vs1_ctx *ctx = platform_get_drvdata(pdev);

	devm_pinctrl_put(ctx->pinctrl);
	devm_kfree(ctx->dev, ctx);
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
		pr_err("Unable to register vs1 platform driver:%d\n", rc);
		return rc;
	}

	rc = platform_driver_register(&fan_driver);
	if (rc) {
		pr_err("Unable to register fan platform driver:%d\n", rc);
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
