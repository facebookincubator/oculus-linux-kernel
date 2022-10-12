/*
 * pwm-tach-fan.c - Hwmon driver for fans connected to PWM lines.
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Author: Kamil Debski <k.debski@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>

#include <drm/drm_panel.h>

#define CLOSE_LOOP_FAN_FAILURE_TIME_MS 800
#define MAX_FAN_RESET_TRY_COUNT 5
#define FAN_STARTUP_TIME_MS 800
#define MIN_PWM 15
#define COLD_BOOT_PWM 84
#define MAX_PWM 255
#define MAX_STR_LEN 10
#define MAX_RPM_HISTORY 3

#define MID(a, b, c) ((max((a), (b)) > (c)) ? \
		max(min((a), (b)), (c)) : max((a), (b)))

struct pwm_fan_ctx {
	/*
	 * lock must be acquired to call set_pwm_locked, set_rpm_locked, or to
	 * modify any of the rpm/pwm state variables or values variables.
	 */
	struct mutex lock;
	struct pwm_device *pwm;
	struct hrtimer fan_timer;
	struct thermal_cooling_device *cdev;
	struct workqueue_struct *wq;
	struct work_struct fan_work;
	struct work_struct fan_recovery_work;
	struct notifier_block fb_notif;
	unsigned int pwm_value;
	unsigned int pwm_fan_state;
	unsigned int pwm_fan_max_state;
	unsigned int *pwm_fan_cooling_levels;
	int tach_gpio;
	unsigned int irq;
	u64 tach_periods;
	atomic64_t rpm;
	ktime_t last_tach_timestamp;
	int max_rpm;
	int rpm_value;
	int rpm_history[MAX_RPM_HISTORY];
	int resume_rpm_value;
	bool is_display_on;
	u64 timer_ticks;
	bool force_failure;
	int reset_count;
};

static struct drm_panel *active_panel;

static ktime_t get_rpm_delay(int rpm)
{
	if (rpm > 2000)
		return ms_to_ktime(50);
	else if (rpm >= 1100)
		return ms_to_ktime(100);
	else if (rpm >= 600)
		return ms_to_ktime(150);
	else if (rpm >= 0)
		return ms_to_ktime(200);
	return 0;
}

static unsigned int get_tolerance(int rpm)
{
	if (rpm > 4500)
		return 200;
	else if (rpm >= 4500)
		return 180;
	else if (rpm >= 3500)
		return 150;
	else if (rpm >= 2500)
		return 100;
	else if (rpm >= 1500)
		return 80;
	else if (rpm >= 800)
		return 60;
	else if (rpm >= 500)
		return 50;
	return 200;
}

static void reset_counters(struct pwm_fan_ctx *ctx)
{
	memset(ctx->rpm_history, 0, sizeof(ctx->rpm_history));
	ctx->timer_ticks = 0;
	ctx->tach_periods = 0;
	ctx->last_tach_timestamp = ktime_get();
}

static int enable_fan(struct pwm_fan_ctx *ctx)
{
	int ret = pwm_enable(ctx->pwm);

	if (ret)
		return ret;
	reset_counters(ctx);
	enable_irq(ctx->irq);
	/* Allow fan enough time to start from idle */
	hrtimer_start(&ctx->fan_timer,
			ms_to_ktime(FAN_STARTUP_TIME_MS),
			HRTIMER_MODE_REL);

	return 0;
}

static void disable_fan(struct pwm_fan_ctx *ctx)
{
	hrtimer_cancel(&ctx->fan_timer);
	cancel_work_sync(&ctx->fan_work);
	disable_irq(ctx->irq);
	pwm_disable(ctx->pwm);
	atomic64_set(&ctx->rpm, 0);
}

static void reset_fan(struct pwm_fan_ctx *ctx)
{
	disable_fan(ctx);
	enable_fan(ctx);
}

static int set_pwm_locked(struct pwm_fan_ctx *ctx, unsigned long pwm)
{
	unsigned long duty;
	unsigned long period;
	ssize_t ret = 0;

	if (ctx->pwm_value == pwm)
		return ret;

	if (pwm == 0) {
		disable_fan(ctx);
		goto set_pwm_success;
	}

	period = ctx->pwm->args.period;
	duty = DIV_ROUND_UP(pwm * (period - 1), MAX_PWM);

	ret = pwm_config(ctx->pwm, duty, period);
	if (ret)
		return ret;

	if (ctx->pwm_value == 0) {
		ret = enable_fan(ctx);
		if (ret)
			return ret;
	}

set_pwm_success:
	ctx->pwm_value = pwm;
	return ret;
}

static int set_rpm_locked(struct pwm_fan_ctx *ctx, unsigned long rpm)
{
	ssize_t ret = 0;
	int current_rpm_value = 0;

	current_rpm_value = ctx->rpm_value;
	ctx->rpm_value = rpm;

	if (rpm == 0)
		/* Setting PWM to 0 always returns 0 */
		return set_pwm_locked(ctx, 0);

	if (current_rpm_value == 0) {
		/* Start fan at 1/3 speed so it can start */
		ret = set_pwm_locked(ctx, 33 * MAX_PWM / 100);
		if (ret)
			ctx->rpm_value = current_rpm_value;
	}

	return ret;
}

static ssize_t set_force_failure(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long force_failure;

	if (kstrtoul(buf, 10, &force_failure))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ctx->force_failure = (force_failure != 0);
	mutex_unlock(&ctx->lock);

	return count;
}

static ssize_t show_force_failure(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int ret;

	mutex_lock(&ctx->lock);
	ret = snprintf(buf, MAX_STR_LEN, "%u\n", ctx->force_failure);
	mutex_unlock(&ctx->lock);
	return ret;
}

static ssize_t set_pwm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long pwm;
	ssize_t ret;

	if (kstrtoul(buf, 10, &pwm) || pwm > MAX_PWM)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ret = set_pwm_locked(ctx, pwm);
	mutex_unlock(&ctx->lock);
	if (ret)
		return ret;

	return count;
}

static ssize_t show_pwm(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	u32 pwm_value;

	mutex_lock(&ctx->lock);
	pwm_value = ctx->pwm_value;
	mutex_unlock(&ctx->lock);

	return snprintf(buf, MAX_STR_LEN, "%u\n", pwm_value);
}

static ssize_t set_rpm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned long rpm;
	ssize_t ret;

	if (kstrtoul(buf, 10, &rpm))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ret = set_rpm_locked(ctx, rpm);
	mutex_unlock(&ctx->lock);
	if (ret)
		return ret;

	return count;
}

static ssize_t show_rpm(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	return snprintf(buf, MAX_STR_LEN, "%ld\n", atomic64_read(&ctx->rpm));
}

/*
 * create sys node in sys/class/hwmon/hwmon2/
 * pwm & rpm
 */
static SENSOR_DEVICE_ATTR(force_failure, 0600, show_force_failure, set_force_failure, 0);
static SENSOR_DEVICE_ATTR(pwm, 0644, show_pwm, set_pwm, 0);
static SENSOR_DEVICE_ATTR(rpm, 0644, show_rpm, set_rpm, 0);

static struct attribute *pwm_fan_attrs[] = {
	&sensor_dev_attr_force_failure.dev_attr.attr,
	&sensor_dev_attr_pwm.dev_attr.attr,
	&sensor_dev_attr_rpm.dev_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(pwm_fan);

static int pwm_fan_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct pwm_fan_ctx *ctx = cdev->devdata;

	if (!ctx)
		return -EINVAL;

	*state = ctx->pwm_fan_max_state;

	return 0;
}

/* Return true iff the fan is in a failure state. */
static bool pwm_fan_has_failure_locked(struct pwm_fan_ctx *ctx)
{
	s64 elapsed_ms;
	u32 pwm;

	elapsed_ms = ktime_to_ms(ktime_sub(ktime_get(), ctx->last_tach_timestamp));
	pwm = ctx->pwm_value;

	/* Wait for a few cycles to report a failure due to PWM not causing an
	 * activity.  The fan might take some time to ramp up.
	 */
	if ((pwm != 0) && (elapsed_ms > CLOSE_LOOP_FAN_FAILURE_TIME_MS))
		return true;

	return false;
}

static int pwm_fan_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct pwm_fan_ctx *ctx = cdev->devdata;

	if (!ctx)
		return -EINVAL;

	mutex_lock(&ctx->lock);
	if (!ctx->is_display_on)
		*state = 0;
	else if (ctx->force_failure || ctx->reset_count >= MAX_FAN_RESET_TRY_COUNT)
		/* If fan is in force_failure mode or if fan recovery failed,
		 * set a state that exceeds the maximum to signal userspace of
		 * fan mal-function.
		 */
		*state = ctx->pwm_fan_max_state + 1;
	else
		*state = ctx->pwm_fan_state;
	mutex_unlock(&ctx->lock);

	return 0;
}

static int
pwm_fan_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct pwm_fan_ctx *ctx = cdev->devdata;
	int ret = 0;

	if (!ctx || (state > ctx->pwm_fan_max_state))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	if (state == ctx->pwm_fan_state)
		goto end_set_cur_state;

	if (ctx->is_display_on) {
		ret = set_rpm_locked(ctx, ctx->pwm_fan_cooling_levels[state]);
		if (ret) {
			dev_err(&cdev->device, "Cannot set pwm!\n");
			goto end_set_cur_state;
		}
	} else {
		/* Set RPM to expected level once display is on */
		ctx->resume_rpm_value = ctx->pwm_fan_cooling_levels[state];
	}

	ctx->pwm_fan_state = state;

end_set_cur_state:
	mutex_unlock(&ctx->lock);
	return ret;
}

static const struct thermal_cooling_device_ops pwm_fan_cooling_ops = {
	.get_max_state = pwm_fan_get_max_state,
	.get_cur_state = pwm_fan_get_cur_state,
	.set_cur_state = pwm_fan_set_cur_state,
};

#ifdef CONFIG_DRM
static int pwm_fan_fb_notifier_cb(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pwm_fan_ctx *ctx =
		container_of(nb, struct pwm_fan_ctx, fb_notif);
	struct drm_panel_notifier *evdata = data;
	int current_rpm_value = 0;
	int action = 0;

	if (!evdata || !evdata->data)
		return 0;

	if (event != DRM_PANEL_EVENT_BLANK)
		return 0;

	action = *(int *)evdata->data;

	mutex_lock(&ctx->lock);
	switch (action) {
	case DRM_PANEL_BLANK_POWERDOWN:
	case DRM_PANEL_BLANK_LP:
		current_rpm_value = ctx->rpm_value;
		if (current_rpm_value > 0) {
			set_rpm_locked(ctx, 0);
		}
		ctx->resume_rpm_value = current_rpm_value;
		ctx->is_display_on = false;
		break;
	case DRM_PANEL_BLANK_UNBLANK:
		current_rpm_value = ctx->resume_rpm_value;
		if (current_rpm_value > 0)
			set_rpm_locked(ctx, current_rpm_value);
		ctx->is_display_on = true;
		break;
	}
	mutex_unlock(&ctx->lock);

	return 0;
}
#endif

static irqreturn_t pwm_fan_irq_handler(int irq, void *dev_id)
{
	struct pwm_fan_ctx *ctx = dev_id;
	ktime_t curr_time = ktime_get();

	BUG_ON(irq != ctx->irq);

	/* Failures are simulated by ignoring tach interrupts from the fan. */
	if (ctx->force_failure)
		return IRQ_HANDLED;

	ctx->tach_periods++;

	/*
	 * T = T1 + T2 + T3 + T4 = 60 / N (Sec)  N:SPEED (RPM)
	 * One period has three rising edge interrupts, therefore
	 * 3 rotation = tach_periods / 6
	 * Refer to ND35C04-19F19-318002200012-REV01
	 * RPM = (tach_periods / 6) * 60 * 1000 * 1000 / (elapsed_us / 3)
	 */
	if ((ctx->tach_periods % 6) == 0) {
		s64 elapsed_us = ktime_to_us(ktime_sub(
					curr_time, ctx->last_tach_timestamp));
		ctx->last_tach_timestamp = curr_time;

		/* Instant RPM: (60 * 1000 * 1000) us * 3rot / elapsed_us */
		atomic64_set(&ctx->rpm, 60 * 3 * 1000 * 1000 / elapsed_us);
	}

	return IRQ_HANDLED;
}

static void fan_recovery_work_func(struct work_struct *work)
{
	struct pwm_fan_ctx *ctx = container_of(work, struct pwm_fan_ctx,
			fan_recovery_work);
	int pwm;

	mutex_lock(&ctx->lock);

	pwm = ctx->pwm_value;

	dev_dbg(&ctx->cdev->device, "%s: reset_count %d, pwm %d rpm_value %d\n",
		__func__, ctx->reset_count, pwm, ctx->rpm_value);

	if (ctx->reset_count == 0) {
		reset_fan(ctx);
		ctx->reset_count++;
	} else if (ctx->reset_count < MAX_FAN_RESET_TRY_COUNT) {
		/* try increasing PWM */
		pwm += 10;
		if (pwm > MAX_PWM)
			pwm = MAX_PWM;

		if (pwm < COLD_BOOT_PWM)
			pwm = COLD_BOOT_PWM;

		set_pwm_locked(ctx, pwm);
		ctx->reset_count++;
	}

	mutex_unlock(&ctx->lock);
}

static void fan_work_func(struct work_struct *work)
{
	struct pwm_fan_ctx *ctx = container_of(work, struct pwm_fan_ctx,
			fan_work);
	int rpm_mid = 0;
	int pwm;
	int rpm_value;
	int rpm_history_idx = ctx->timer_ticks;
	int tolerance;
	bool fan_failed;

	if (!mutex_trylock(&ctx->lock))
		return;

	pwm = ctx->pwm_value;
	rpm_value = ctx->rpm_value;

	fan_failed = pwm_fan_has_failure_locked(ctx);

	dev_dbg(&ctx->cdev->device, "%s: force_failure %d, fan_failed %d\n",
		__func__, ctx->force_failure, fan_failed);

	if (ctx->force_failure) {
		/* Forced fan failure, do nothing */
		goto end_work_func;
	}

	if (fan_failed) {
		queue_work(ctx->wq, &ctx->fan_recovery_work);
		goto end_work_func;
	}

	if (ctx->reset_count != 0) {
		dev_dbg(&ctx->cdev->device, "Fan recovered after %d attempts\n",
			ctx->reset_count);
		ctx->reset_count = 0;
	}

	/*
	 * Record current RPM value
	 * This requires timer interval >> IRQ interval
	 */
	rpm_history_idx = (rpm_history_idx % MAX_RPM_HISTORY);
	ctx->rpm_history[rpm_history_idx] = atomic64_read(&ctx->rpm);
	ctx->timer_ticks++;

	/* Require filled RPM history buffer to determine median */
	if (ctx->timer_ticks < MAX_RPM_HISTORY)
		goto end_work_func;

	/* Take median of last 3 historical RPM values */
	/* TODO(ethanc): Take median across N samples */
	rpm_mid = MID(ctx->rpm_history[rpm_history_idx % MAX_RPM_HISTORY],
		ctx->rpm_history[(rpm_history_idx - 1) % MAX_RPM_HISTORY],
		ctx->rpm_history[(rpm_history_idx - 2) % MAX_RPM_HISTORY]);

	/*
	 * To make the actual rpm closer to the set value
	 * If set value is greater than 2000, tolerance set
	 * to 200, otherwise set to 10% of set value
	 */
	tolerance = get_tolerance(rpm_value);
	if (abs(rpm_mid - rpm_value) > tolerance) {
		pwm = (rpm_mid > rpm_value) ? (pwm - 1) : (pwm + 1);
		/* Restrict to MIN_PWM to MAX_PWM */
		pwm = max(min(MAX_PWM, pwm), MIN_PWM);
		set_pwm_locked(ctx, pwm);
	}
end_work_func:
	mutex_unlock(&ctx->lock);
}

static enum hrtimer_restart fan_timer_func(struct hrtimer *timer)
{
	struct pwm_fan_ctx *ctx = container_of(timer, struct pwm_fan_ctx,
			fan_timer);

	queue_work(ctx->wq, &ctx->fan_work);
	hrtimer_forward_now(&ctx->fan_timer,
			get_rpm_delay(atomic64_read(&ctx->rpm)));
	return HRTIMER_RESTART;
}

static int pwm_fan_of_get_cooling_data(struct device *dev,
				       struct pwm_fan_ctx *ctx)
{
	struct device_node *np = dev->of_node;
	int num, i, ret;

	if (!of_find_property(np, "cooling-levels", NULL))
		return 0;

	ret = of_property_count_u32_elems(np, "cooling-levels");
	if (ret <= 0) {
		dev_err(dev, "invalid cooling-levels property!!\n");
		return ret ? : -EINVAL;
	}

	num = ret;
	ctx->pwm_fan_cooling_levels = devm_kcalloc(dev, num, sizeof(u32),
						   GFP_KERNEL);
	if (!ctx->pwm_fan_cooling_levels)
		return -ENOMEM;

	ret = of_property_read_u32_array(np, "cooling-levels",
					 ctx->pwm_fan_cooling_levels, num);
	if (ret) {
		dev_err(dev, "Property 'cooling-levels' cannot be read!\n");
		return ret;
	}

	/* Cooling levels are expressed in RPM */
	for (i = 0; i < num; i++) {
		if (ctx->pwm_fan_cooling_levels[i] > ctx->max_rpm) {
			dev_err(dev, "RPM fan state[%d]:%d > %d\n", i,
				ctx->pwm_fan_cooling_levels[i], ctx->max_rpm);
			return -EINVAL;
		}
	}

	ctx->pwm_fan_max_state = num - 1;

	return 0;
}

#ifdef CONFIG_DRM
static int pwm_fan_set_active_panel(struct device_node *np)
{
	int i, count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return 0;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			active_panel = panel;
			return 0;
		}
	}

	return -ENODEV;
}
#endif

static int pwm_fan_probe(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev;
	struct device *hwmon;
	struct pwm_fan_ctx *ctx;

	int ret;
	u32 dt_addr;

#ifdef CONFIG_DRM
	ret = pwm_fan_set_active_panel(pdev->dev.of_node);
	if (ret) {
		dev_warn(&pdev->dev,
			"No active panel, deferring probe");
		return -EPROBE_DEFER;
	}
#endif

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->wq = alloc_workqueue("fan_wq", WQ_UNBOUND, 1);
	if (!ctx->wq) {
		dev_err(&pdev->dev, "%s: could not create workqueue\n",
				__func__);
		return -ENOMEM;
	}

	dev_dbg(&pdev->dev, "enter pwm fan probe\n");

	INIT_WORK(&ctx->fan_work, fan_work_func);
	INIT_WORK(&ctx->fan_recovery_work, fan_recovery_work_func);
	mutex_init(&ctx->lock);
	hrtimer_init(&ctx->fan_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ctx->fan_timer.function = fan_timer_func;

	/* Get the tach gpio and config it to INPUT */
	ctx->tach_gpio = of_get_named_gpio(pdev->dev.of_node,
		"fan,tach-gpio", 0);
	if (!gpio_is_valid(ctx->tach_gpio)) {
		dev_err(&pdev->dev, "tach gpio is invalid\n");
		return -EINVAL;
	}
	ret = devm_gpio_request_one(&pdev->dev, ctx->tach_gpio,
		GPIOF_IN, "fan_tach_gpio");
	if (ret) {
		dev_err(&pdev->dev, "devm_gpio_request_one for tach_gpio failed\n");
		goto err_tach_gpio_dir;
	}

	ctx->irq = gpio_to_irq(ctx->tach_gpio);
	if (ctx->irq < 0) {
		dev_err(&pdev->dev, "gpio_to_irq for ctx->irq failed %d\n",
			ctx->irq);
		ret = ctx->irq;
		goto err_tach_gpio_dir;
	}

	ret = devm_request_threaded_irq(&pdev->dev, ctx->irq,
				  NULL, pwm_fan_irq_handler,
				  IRQF_TRIGGER_RISING | IRQF_ONESHOT,
				  "pwm-tach-fan", ctx);
	if (ret) {
		dev_err(&pdev->dev, "devm_request_threaded_irq failed\n");
		goto err_tach_gpio_dir;
	}

	/* Disable irq first & enable_irq in sys node */
	disable_irq(ctx->irq);
	reset_counters(ctx);

	ctx->pwm = devm_of_pwm_get(&pdev->dev, pdev->dev.of_node, NULL);
	if (IS_ERR(ctx->pwm)) {
		dev_err(&pdev->dev, "Could not get PWM\n");
		ret = PTR_ERR(ctx->pwm);
		goto err_tach_gpio_dir;
	}

	platform_set_drvdata(pdev, ctx);

	hwmon = devm_hwmon_device_register_with_groups(&pdev->dev, "pwmfan",
						       ctx, pwm_fan_groups);
	if (IS_ERR(hwmon)) {
		dev_err(&pdev->dev, "Failed to register hwmon device\n");
		ret = PTR_ERR(hwmon);
		goto err_tach_gpio_dir;
	}

	ctx->is_display_on = true;
	ctx->reset_count = 0;
#ifdef CONFIG_DRM
	ctx->fb_notif.notifier_call = pwm_fan_fb_notifier_cb;
	if (active_panel) {
		ret = drm_panel_notifier_register(active_panel,
				&ctx->fb_notif);
		if (ret)
			goto err_tach_gpio_dir;
	}
#endif

	ret = of_property_read_u32(pdev->dev.of_node, "max-rpm", &ctx->max_rpm);
	if (ret) {
		dev_err(&pdev->dev, "Property 'max-rpm' cannot be read!\n");
		goto err_tach_gpio_dir;
	}

	ret = pwm_fan_of_get_cooling_data(&pdev->dev, ctx);
	if (ret)
		goto err_tach_gpio_dir;

	ret = of_property_read_u32(pdev->dev.of_node, "reg", &dt_addr);
	if (ret)
		goto err_tach_gpio_dir;

	ctx->pwm_fan_state = ctx->pwm_fan_max_state;
	if (IS_ENABLED(CONFIG_THERMAL)) {
		char cdev_name[THERMAL_NAME_LENGTH] = "";

		snprintf(cdev_name, THERMAL_NAME_LENGTH, "pwm-tach-fan%d",
				dt_addr);
		cdev = thermal_of_cooling_device_register(pdev->dev.of_node,
							  cdev_name, ctx,
							  &pwm_fan_cooling_ops);
		if (IS_ERR(cdev)) {
			dev_err(&pdev->dev,
				"Failed to register %s as cooling device",
				cdev_name);
			ret = PTR_ERR(cdev);
			goto err_tach_gpio_dir;
		}
		ctx->cdev = cdev;
		thermal_cdev_update(cdev);
	}

	return 0;

err_tach_gpio_dir:
	pwm_disable(ctx->pwm);
	return ret;
}

static int pwm_fan_remove(struct platform_device *pdev)
{
	struct pwm_fan_ctx *ctx = platform_get_drvdata(pdev);

#ifdef CONFIG_DRM
	if (active_panel)
		drm_panel_notifier_unregister(active_panel, &ctx->fb_notif);
#endif

	thermal_cooling_device_unregister(ctx->cdev);
	mutex_lock(&ctx->lock);
	if (ctx->pwm_value)
		disable_fan(ctx);
	mutex_unlock(&ctx->lock);

	destroy_workqueue(ctx->wq);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int pwm_fan_suspend(struct device *dev)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int current_rpm_value;
	int ret = 0;

	if (active_panel)
		/* On/off controlled by panel state instead */
		return 0;

	mutex_lock(&ctx->lock);
	current_rpm_value = ctx->rpm_value;
	if (current_rpm_value > 0) {
		ctx->resume_rpm_value = current_rpm_value;

		ret = set_rpm_locked(ctx, 0);
	}
	mutex_unlock(&ctx->lock);

	return ret;
}

static int pwm_fan_resume(struct device *dev)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int current_rpm_value;
	int ret = 0;

	if (active_panel)
		/* On/off controlled by panel state instead */
		return 0;

	mutex_lock(&ctx->lock);
	current_rpm_value = ctx->resume_rpm_value;
	if (current_rpm_value == 0)
		goto end_pwm_fan_resume;

	ret = set_rpm_locked(ctx, current_rpm_value);

end_pwm_fan_resume:
	mutex_unlock(&ctx->lock);
	return ret;
}
#else
#define pwm_fan_suspend NULL
#define pwm_fan_resume NULL
#endif

static SIMPLE_DEV_PM_OPS(pwm_fan_pm, pwm_fan_suspend, pwm_fan_resume);

static const struct of_device_id of_pwm_tach_fan_match[] = {
	{ .compatible = "pwm-tach-fan", },
	{},
};
MODULE_DEVICE_TABLE(of, of_pwm_tach_fan_match);

static struct platform_driver pwm_fan_driver = {
	.probe	= pwm_fan_probe,
	.remove	= pwm_fan_remove,
	.driver	= {
		.name	= "pwm-tach-fan",
		.pm	= &pwm_fan_pm,
		.of_match_table	= of_pwm_tach_fan_match,
	},
};

static int __init pwm_fan_init(void)
{
	return platform_driver_register(&pwm_fan_driver);
}

static void __exit pwm_fan_exit(void)
{
	platform_driver_unregister(&pwm_fan_driver);
}

late_initcall(pwm_fan_init);
module_exit(pwm_fan_exit);

MODULE_ALIAS("platform:pwm-tach-fan");
MODULE_DESCRIPTION("PWM TACH FAN driver");
MODULE_LICENSE("GPL");
