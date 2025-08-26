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
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
#include <linux/soc/qcom/panel_event_notifier.h>
#endif
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>
#include <linux/thermal.h>

#include <drm/drm_panel.h>

#define RECOVERY_STEP_SIZE 50
#define FAN_MIN_OFF_TIME_MS 400
#define FAN_CHECK_PERIOD_MS 5000
#define FAN_STALL_DETECT_TIME_MS (FAN_CHECK_PERIOD_MS + 1000)
#define FAN_STALL_REPORT_TIME_MS 9000 /* starting after stall detection */
#define FAN_STARTUP_IRQ_IGNORE_TIME_MS 2300
#define COLD_BOOT_PWM 84U
#define FORCE_FAILURE_PWM 0U
#define DEFAULT_VDD_TO_PWM_DELAY_MS 0U
#define DEFAULT_RPM_PER_SEC 0U /* 0 = instant ramp */
#define DEFAULT_SILENT_RPM 0 /* below this, fan is considered inaudible */
#define DEFAULT_MIN_PWM 15U
#define DEFAULT_MAX_PWM 255U
#define MAX_HW_PWM 255U
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
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;
	struct pinctrl_state *idle_state;
	struct thermal_cooling_device *cdev;
	struct regulator *vdd_supply;
	struct workqueue_struct *wq;
	struct delayed_work fan_dwork;
	struct work_struct fan_recovery_work;
#if IS_ENABLED(CONFIG_DRM)
	bool use_panel_notifiers;
	struct notifier_block fb_notif;
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	void *notifier_cookie;
#else
	struct drm_panel *active_panel;
#endif
#endif
	int32_t cold_boot_pwm;
	int32_t min_pwm;
	int32_t max_pwm;
	int32_t pwm_value;
	int32_t recovery_pwm_value;
	int32_t rpm_per_sec;
	int32_t silent_rpm;
	int32_t vdd_to_pwm_delay_ms;
	int32_t pwm_fan_state;
	int32_t pwm_fan_max_state;
	int32_t *pwm_fan_cooling_levels;
	uint32_t irq;
	u64 tach_periods;
	atomic64_t rpm;
	ktime_t last_disable_timestamp;
	ktime_t last_enable_timestamp;
	ktime_t last_stall_start_timestamp;
	ktime_t last_stall_detect_timestamp;
	ktime_t last_tach_timestamp;
	ktime_t last_rpm_update_timestamp;
	int32_t max_rpm;
	int32_t rpm_value;
	int32_t target_rpm_value;
	int32_t rpm_history[MAX_RPM_HISTORY];
	int32_t resume_rpm_value;
	bool is_display_on;
	u64 timer_ticks;
	bool force_failure;
	bool ignore_tach_irqs;
	bool fan_stalled;
	bool vdd_enabled;
	int reset_count;
	bool target_acquired;
};

static uint32_t get_rpm_delay_ms(int32_t rpm)
{
	if (rpm > 2000)
		return 50;
	else if (rpm >= 1100)
		return 100;
	else if (rpm >= 600)
		return 150;
	else if (rpm >= 0)
		return 200;
	return 0;
}

static int32_t get_tolerance(int32_t rpm)
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
	ctx->ignore_tach_irqs = true;
	ctx->timer_ticks = 0;
	ctx->tach_periods = 0;
}

static int enable_fan_notimestamp_locked(struct pwm_fan_ctx *ctx)
{
	int ret;
	ktime_t min_enable_time;
	s64 delay_ms = 0;

	if (ctx->vdd_supply != NULL && !ctx->vdd_enabled) {
		ret = regulator_enable(ctx->vdd_supply);
		if (ret < 0) {
			dev_err(&ctx->cdev->device, "regulator enable failed: %d\n", ret);
			return ret;
		}
		ctx->vdd_enabled = true;
		delay_ms = ctx->vdd_to_pwm_delay_ms;
	}

	/*
	 * Wait at least FAN_MIN_OFF_TIME_MS since last disable.
	 * If a vdd supply is specificed, make sure we wait at least vdd_to_pwm_delay_ms
	 * after enabling the regulator, per fan datasheet requirements.
	 */
	min_enable_time = ktime_add_ms(ctx->last_disable_timestamp, FAN_MIN_OFF_TIME_MS);
	delay_ms = max(delay_ms, ktime_to_ms(ktime_sub(min_enable_time, ktime_get())));
	if (delay_ms > 0)
		msleep(delay_ms);

	ret = pwm_enable(ctx->pwm);
	if (ret)
		return ret;

	if (ctx->active_state) {
		ret = pinctrl_select_state(ctx->pinctrl, ctx->active_state);
		if (ret < 0) {
			dev_err(&ctx->cdev->device, "failed to apply active pin state: %d\n", ret);
			return ret;
		}
	}

	reset_counters(ctx);
	enable_irq(ctx->irq);
	/* Allow fan enough time to start from idle */
	queue_delayed_work(ctx->wq, &ctx->fan_dwork,
			   msecs_to_jiffies(FAN_STARTUP_IRQ_IGNORE_TIME_MS));

	return 0;
}

static int enable_fan_locked(struct pwm_fan_ctx *ctx)
{
	int ret;

	ret = enable_fan_notimestamp_locked(ctx);
	if (!ret)
		ctx->last_enable_timestamp = ktime_get();

	return ret;
}

static void disable_fan_notimestamp_locked(struct pwm_fan_ctx *ctx)
{
	int rc;

	cancel_delayed_work_sync(&ctx->fan_dwork);
	disable_irq(ctx->irq);
	if (ctx->idle_state) {
		rc = pinctrl_select_state(ctx->pinctrl, ctx->idle_state);
		if (rc < 0)
			dev_err(&ctx->cdev->device, "failed to apply idle pin state: %d\n", rc);
	}
	pwm_disable(ctx->pwm);
	atomic64_set(&ctx->rpm, 0);

	if (ctx->vdd_supply != NULL && ctx->vdd_enabled) {
		int rc = regulator_disable(ctx->vdd_supply);
		if (rc < 0) {
			dev_err(&ctx->cdev->device, "regulator disable failed: %d\n", rc);
			return;
		}
		ctx->vdd_enabled = false;
	}
}

static void disable_fan_locked(struct pwm_fan_ctx *ctx)
{
	disable_fan_notimestamp_locked(ctx);
	ctx->last_disable_timestamp = ktime_get();
}

static int set_pwm_locked(struct pwm_fan_ctx *ctx, int32_t pwm)
{
	int32_t target_pwm;
	int32_t duty;
	int32_t period;
	ssize_t ret = 0;

	if (ctx->pwm_value == pwm && !ctx->fan_stalled)
		return ret;

	if (pwm == 0) {
		disable_fan_locked(ctx);
		goto set_pwm_success;
	}

	target_pwm = ctx->force_failure ? FORCE_FAILURE_PWM : pwm;

	period = ctx->pwm->args.period;
	duty = DIV_ROUND_UP(target_pwm * (period - 1), MAX_HW_PWM);

	ret = pwm_config(ctx->pwm, duty, period);
	if (ret)
		return ret;

	if (ctx->pwm_value == 0) {
		ret = enable_fan_locked(ctx);
		if (ret)
			return ret;
	}

set_pwm_success:
	ctx->pwm_value = pwm;
	return ret;
}

static int set_rpm_locked(struct pwm_fan_ctx *ctx, int32_t rpm, bool skip_ramp)
{
	ssize_t ret = 0;
	int32_t prev_target_rpm_value = ctx->target_rpm_value;
	int32_t prev_rpm_value = ctx->rpm_value;

	if (rpm > ctx->max_rpm) {
		rpm = ctx->max_rpm;
	}

	ctx->target_rpm_value = rpm;

	if (rpm == 0) {
		/* Disable immediately, without ramping from target */
		ctx->rpm_value = 0;

		/* Setting PWM to 0 always returns 0 */
		return set_pwm_locked(ctx, 0);
	}

	/* rpm_per_sec of 0 means the ramp should be instantaneous. */
	if (skip_ramp || ctx->rpm_per_sec == 0)
		ctx->rpm_value = ctx->target_rpm_value;

	if (prev_target_rpm_value == 0) {
		/* Start fan at "cold boot" speed so it can start */
		ret = set_pwm_locked(ctx, ctx->cold_boot_pwm);
		if (ret) {
			ctx->target_rpm_value = prev_target_rpm_value;
			ctx->rpm_value = prev_rpm_value;
		}
	}

	return ret;
}

static void reset_fan_locked(struct pwm_fan_ctx *ctx, int32_t pwm)
{
	/*
	 * Toggle fan off and back on. Do this without updating the disable timestamp,
	 * so fan failure state isn't reset.
	 */
	disable_fan_notimestamp_locked(ctx);
	set_pwm_locked(ctx, pwm);
	enable_fan_notimestamp_locked(ctx);
	ctx->reset_count++;
}

static ssize_t set_force_failure(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned int force_failure;

	if (kstrtouint(buf, 10, &force_failure))
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
	int32_t pwm;
	ssize_t ret;

	if (kstrtouint(buf, 10, &pwm) || pwm > ctx->max_pwm)
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

	uint32_t pwm_value;

	mutex_lock(&ctx->lock);
	pwm_value = ctx->pwm_value;
	mutex_unlock(&ctx->lock);

	return snprintf(buf, MAX_STR_LEN, "%d\n", pwm_value);
}

static ssize_t set_rpm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned int rpm;
	ssize_t ret;

	if (kstrtouint(buf, 10, &rpm))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ret = set_rpm_locked(ctx, rpm, false);
	mutex_unlock(&ctx->lock);
	if (ret)
		return ret;

	return count;
}

static ssize_t show_rpm(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	return snprintf(buf, MAX_STR_LEN, "%lld\n", (s64)atomic64_read(&ctx->rpm));
}

static ssize_t set_rpm_per_sec(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned int rpm_per_sec;

	if (kstrtouint(buf, 10, &rpm_per_sec))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ctx->rpm_per_sec = rpm_per_sec;
	mutex_unlock(&ctx->lock);

	return count;
}

static ssize_t show_rpm_per_sec(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	return snprintf(buf, MAX_STR_LEN, "%u\n", ctx->rpm_per_sec);
}

static ssize_t set_silent_rpm(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	unsigned int silent_rpm;

	if (kstrtouint(buf, 10, &silent_rpm))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	ctx->silent_rpm = silent_rpm;
	mutex_unlock(&ctx->lock);

	return count;
}

static ssize_t show_silent_rpm(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);

	return snprintf(buf, MAX_STR_LEN, "%u\n", ctx->silent_rpm);
}

/*
 * create sys node in sys/class/hwmon/hwmon2/
 * pwm & rpm
 */
static SENSOR_DEVICE_ATTR(force_failure, 0600, show_force_failure, set_force_failure, 0);
static SENSOR_DEVICE_ATTR(pwm, 0644, show_pwm, set_pwm, 0);
static SENSOR_DEVICE_ATTR(rpm, 0644, show_rpm, set_rpm, 0);
static SENSOR_DEVICE_ATTR(rpm_per_sec, 0644, show_rpm_per_sec, set_rpm_per_sec, 0);
static SENSOR_DEVICE_ATTR(silent_rpm, 0644, show_silent_rpm, set_silent_rpm, 0);

static struct attribute *pwm_fan_attrs[] = {
	&sensor_dev_attr_force_failure.dev_attr.attr,
	&sensor_dev_attr_pwm.dev_attr.attr,
	&sensor_dev_attr_rpm.dev_attr.attr,
	&sensor_dev_attr_rpm_per_sec.dev_attr.attr,
	&sensor_dev_attr_silent_rpm.dev_attr.attr,
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
	s64 delta_ms;
	ktime_t now = ktime_get();

	/* Fan is disabled? Not failed. */
	if (ctx->pwm_value == 0)
		return false;

	/* Tach activity in last FAN_STALL_DETECT_TIME_MS? Not failed. */
	delta_ms = ktime_to_ms(ktime_sub(now, ctx->last_tach_timestamp));
	if (delta_ms < FAN_STALL_DETECT_TIME_MS)
		return false;

	/* Fan has not been enabled for at least FAN_STALL_DETECT_TIME_MS? Not failed. */
	delta_ms = ktime_to_ms(ktime_sub(now, ctx->last_enable_timestamp));
	if (delta_ms < FAN_STALL_DETECT_TIME_MS)
		return false;

	return true;
}

static int pwm_fan_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct pwm_fan_ctx *ctx = cdev->devdata;

	if (!ctx)
		return -EINVAL;

	mutex_lock(&ctx->lock);

	/*
	 * Report fan malfunction if fan has been stalled for
	 * at least FAN_STALL_REPORT_TIME_MS.
	 */
	if (ctx->fan_stalled) {
		s64 time_since_stall_ms = ktime_to_ms(ktime_sub(ktime_get(), ctx->last_stall_start_timestamp));
		if (time_since_stall_ms > FAN_STALL_REPORT_TIME_MS) {
			/*
			 * If fan has not recovered, set a state that exceeds the maximum
			 * to signal userspace of the fan malfunction.
			 */
			*state = ctx->pwm_fan_max_state + 1;
			goto end_get_cur_state;
		}
	}

	/* Otherwise, report currently set state.  */
	*state = ctx->pwm_fan_state;

end_get_cur_state:
	mutex_unlock(&ctx->lock);

	return 0;
}

static int
pwm_fan_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct pwm_fan_ctx *ctx = cdev->devdata;
	int ret = 0;
	int32_t rpm;

	if (!ctx || (state > ctx->pwm_fan_max_state))
		return -EINVAL;

	mutex_lock(&ctx->lock);
	if (state == ctx->pwm_fan_state)
		goto end_set_cur_state;

	rpm = ctx->pwm_fan_cooling_levels[state];
	if (!ctx->is_display_on) {
		/* Set RPM to expected level once display is on */
		ctx->resume_rpm_value = rpm;

		/* Limit RPM to silent value */
		rpm = min(rpm, ctx->silent_rpm);
	}

	ret = set_rpm_locked(ctx, rpm, false);
	if (ret) {
		dev_err(&cdev->device, "Cannot set rpm!\n");
		goto end_set_cur_state;
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

#if IS_ENABLED(CONFIG_DRM)
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
static void pwm_fan_panel_notifier_cb(enum panel_event_notifier_tag tag,
		struct panel_event_notification *notification, void *client_data)
{
	struct pwm_fan_ctx *ctx =
		container_of(client_data, struct pwm_fan_ctx, fb_notif);
	int32_t current_rpm_value = 0;

	if (!notification)
		return;

	mutex_lock(&ctx->lock);
	switch (notification->notif_type) {
	case DRM_PANEL_EVENT_BLANK:
	case DRM_PANEL_EVENT_BLANK_LP:
		if (!ctx->is_display_on)
			break;
		current_rpm_value = ctx->target_rpm_value;
		if (current_rpm_value > 0)
			set_rpm_locked(ctx, min(current_rpm_value, ctx->silent_rpm), true);
		ctx->resume_rpm_value = current_rpm_value;
		ctx->is_display_on = false;
		break;
	case DRM_PANEL_EVENT_UNBLANK:
		if (ctx->is_display_on)
			break;
		current_rpm_value = ctx->resume_rpm_value;
		if (current_rpm_value > 0)
			set_rpm_locked(ctx, current_rpm_value, false);
		ctx->is_display_on = true;
		break;
	default:
		/* NONE, FPS_CHANGE don't matter to this driver */
		break;
	}
	mutex_unlock(&ctx->lock);
}
#else
static int pwm_fan_fb_notifier_cb(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct pwm_fan_ctx *ctx =
		container_of(nb, struct pwm_fan_ctx, fb_notif);
	struct drm_panel_notifier *evdata = data;
	int32_t current_rpm_value = 0;
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
		if (current_rpm_value > 0)
			set_rpm_locked(ctx, min(current_rpm_value, ctx->silent_rpm), true);
		ctx->resume_rpm_value = current_rpm_value;
		ctx->is_display_on = false;
		break;
	case DRM_PANEL_BLANK_UNBLANK:
		current_rpm_value = ctx->resume_rpm_value;
		if (current_rpm_value > 0)
			set_rpm_locked(ctx, current_rpm_value, false);
		ctx->is_display_on = true;
		break;
	}
	mutex_unlock(&ctx->lock);

	return 0;
}
#endif
#endif

static irqreturn_t pwm_fan_irq_handler(int irq, void *dev_id)
{
	struct pwm_fan_ctx *ctx = dev_id;

	BUG_ON(irq != ctx->irq);

	if (ctx->ignore_tach_irqs)
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
		ktime_t curr_time = ktime_get();
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

	mutex_lock(&ctx->lock);

	if (ctx->pwm_value == 0) {
		ctx->fan_stalled = false;
		goto end_work_func;
	}

	dev_warn(&ctx->cdev->device, "Fan stall recovery attempt %d (force_failure: %d)\n",
		 ctx->reset_count + 1, ctx->force_failure);

	if (ctx->reset_count == 0) {
		ctx->recovery_pwm_value = max(ctx->cold_boot_pwm, ctx->pwm_value);
		reset_fan_locked(ctx, ctx->recovery_pwm_value);
	} else {
		/* try increasing PWM */
		ctx->recovery_pwm_value = min(ctx->recovery_pwm_value + RECOVERY_STEP_SIZE,
					      ctx->max_pwm);
		set_pwm_locked(ctx, ctx->recovery_pwm_value);
		reset_fan_locked(ctx, ctx->recovery_pwm_value);
	}

end_work_func:
	mutex_unlock(&ctx->lock);
}

static int32_t calc_rpm_step(struct pwm_fan_ctx *ctx)
{
	s64 delta_us;

	if (ktime_after(ctx->last_disable_timestamp, ctx->last_rpm_update_timestamp))
		return ctx->rpm_per_sec;

	delta_us = ktime_us_delta(ktime_get(), ctx->last_rpm_update_timestamp);

	return (ctx->rpm_per_sec * delta_us) / USEC_PER_SEC;
}

static void fan_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct pwm_fan_ctx *ctx = container_of(dwork, struct pwm_fan_ctx, fan_dwork);
	int32_t rpm_mid = 0;
	int32_t rpm_history_idx = ctx->timer_ticks;
	int32_t tolerance;
	bool fan_failed;

	if (!mutex_trylock(&ctx->lock))
		goto end;

	/*
	 * Some fans emit spurious tach interrupts during start-up even
	 * if the fan is jammed (ex. Eureka's Delta fan). So that these
	 * don't interfere with stall detection, ignore any interrupts
	 * that come in during the first FAN_STARTUP_IRQ_IGNORE_TIME_MS.
	 */
	ctx->ignore_tach_irqs = false;

	if (ctx->target_acquired) {
		/*
		 * RPM monitoring was paused because the PWM was resulting
		 * in the desired rate last we checked (FAN_CHECK_PERIOD_MS
		 * ago). It's now time to re-evaluate to make sure the RPM
		 * is still on target.
		 */
		dev_dbg(&ctx->cdev->device, "performing fan health check\n");
		ctx->target_acquired = false;
		reset_counters(ctx);
		enable_irq(ctx->irq);

		/*
		 * Return early to give some time for some IRQs to come in,
		 * for a more accurate RPM measurement.
		 */
		goto end_unlock;
	}

	if (ctx->rpm_value != ctx->target_rpm_value) {
		int32_t prev_rpm = ctx->rpm_value;

		if (prev_rpm <= ctx->silent_rpm && ctx->target_rpm_value <= ctx->silent_rpm) {
			/*
			 * If a silent RPM threshold has been configured and both the new and previous
			 * RPM values are below that, jump directly to the new speed without stepping.
			 */
			ctx->rpm_value = ctx->target_rpm_value;
		} else {
			/* Else, ramp towards target rate one step at a time. */
			int32_t rpm_step = calc_rpm_step(ctx);

			if (ctx->rpm_value < ctx->target_rpm_value)
				ctx->rpm_value = min(ctx->rpm_value + rpm_step, ctx->target_rpm_value);
			else if (ctx->rpm_value > ctx->target_rpm_value)
				ctx->rpm_value = max(ctx->rpm_value - rpm_step, ctx->target_rpm_value);
		}

		ctx->last_rpm_update_timestamp = ktime_get();
	}

	fan_failed = pwm_fan_has_failure_locked(ctx);

	dev_dbg(&ctx->cdev->device, "%s: force_failure %d, fan_failed %d\n",
		__func__, ctx->force_failure, fan_failed);

	if (fan_failed) {
		ktime_t now = ktime_get();

		/* Attempt fan recovery just once every FAN_STALL_DETECT_TIME_MS */
		s64 time_since_stall = ktime_to_ms(ktime_sub(now, ctx->last_stall_detect_timestamp));
		if (time_since_stall > FAN_STALL_DETECT_TIME_MS) {
			ctx->last_stall_detect_timestamp = now;
			if (!ctx->fan_stalled) {
				ctx->last_stall_start_timestamp = now;
				ctx->fan_stalled = true;
			}
			queue_work(ctx->wq, &ctx->fan_recovery_work);
		}
		goto end_unlock;
	}

	if (ctx->fan_stalled) {
		dev_warn(&ctx->cdev->device, "Fan stall recovered after %d attempts\n",
			 ctx->reset_count);
		ctx->fan_stalled = false;
	}
	ctx->reset_count = 0;

	/*
	 * Record current RPM value
	 * This requires timer interval >> IRQ interval
	 */
	rpm_history_idx = (rpm_history_idx % MAX_RPM_HISTORY);
	ctx->rpm_history[rpm_history_idx] = atomic64_read(&ctx->rpm);
	ctx->timer_ticks++;

	/* Require filled RPM history buffer to determine median */
	if (ctx->timer_ticks < MAX_RPM_HISTORY)
		goto end_unlock;

	/* Take median of last 3 historical RPM values */
	/* TODO(ethanc): Take median across N samples */
	rpm_mid = MID(ctx->rpm_history[rpm_history_idx % MAX_RPM_HISTORY],
		ctx->rpm_history[abs((rpm_history_idx - 1) % MAX_RPM_HISTORY)],
		ctx->rpm_history[abs((rpm_history_idx - 2) % MAX_RPM_HISTORY)]);

	/*
	 * To make the actual rpm closer to the set value
	 * If set value is greater than 2000, tolerance set
	 * to 200, otherwise set to 10% of set value
	 */
	tolerance = get_tolerance(ctx->rpm_value);
	if (ctx->force_failure || abs(rpm_mid - ctx->rpm_value) > tolerance) {
		int32_t pwm;

		/* PWM needs adjustment as RPM is off-target. */
		ctx->target_acquired = false;

		pwm = (rpm_mid > ctx->rpm_value) ? (ctx->pwm_value - 1) : (ctx->pwm_value + 1);
		/* Restrict to PWM range */
		pwm = max(min(ctx->max_pwm, pwm), ctx->min_pwm);
		set_pwm_locked(ctx, pwm);

		ctx->last_rpm_update_timestamp = ktime_get();

	} else {
		/*
		 * PWM is resulting in desired RPM. RPM IRQ can be disabled
		 * until the next fan status check (ex. FAN_CHECK_PERIOD_MS)
		 */
		dev_dbg(&ctx->cdev->device, "fan rpm reached target\n");
		ctx->target_acquired = true;
		disable_irq(ctx->irq);
	}

end_unlock:
	mutex_unlock(&ctx->lock);
end:
	queue_delayed_work(ctx->wq, &ctx->fan_dwork,
			   msecs_to_jiffies(ctx->target_acquired ?
				FAN_CHECK_PERIOD_MS :
				get_rpm_delay_ms(atomic64_read(&ctx->rpm))));
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
	ctx->pwm_fan_cooling_levels = devm_kcalloc(dev, num, sizeof(*ctx->pwm_fan_cooling_levels),
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

#if IS_ENABLED(CONFIG_DRM)
static int count_panels(struct device_node *np, const char **panels_prop_name)
{
	const char *prop_name = "panels";

	/*
	 * TODO (T204416818): Remove support for deprecated dts definitions
	 *
	 * For now lets support both the old and the new dts nodes. Once all
	 * platforms have moved to the new model, we can get rid of this.
	 */
	if (!of_property_read_bool(np, prop_name))
		prop_name = "panel";

	if (panels_prop_name)
		*panels_prop_name = prop_name;
	return of_count_phandle_with_args(np, prop_name, NULL);
}

#if !IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
static struct drm_panel *pwm_fan_get_active_panel(struct device_node *np)
{
	int i, count;
	struct device_node *node;
	struct drm_panel *panel;
	const char *panels_prop_name = NULL;

	count = count_panels(np, &panels_prop_name);
	if (count <= 0)
		return NULL;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, panels_prop_name, i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel))
			return panel;
	}

	return ERR_PTR(-ENODEV);
}
#endif /* !CONFIG_QCOM_PANEL_EVENT_NOTIFIER  */
#endif /* CONFIG_DRM */

static int pwm_fan_probe(struct platform_device *pdev)
{
	struct thermal_cooling_device *cdev;
	struct device *hwmon;
	struct pwm_fan_ctx *ctx;
	int ret;
	u32 dt_addr;
	ktime_t now;
#if IS_ENABLED(CONFIG_DRM)
	struct device_node *panels_np = NULL;
	struct device_node *np = pdev->dev.of_node;
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	void *cookie;
#else
	struct drm_panel *panel;
#endif /* CONFIG_QCOM_PANEL_EVENT_NOTIFIER */
#endif /* CONFIG_DRM */

#if IS_ENABLED(CONFIG_DRM)
	panels_np = of_parse_phandle(np, "panels", 0);
	if (panels_np)
		np = panels_np;
#if !IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	panel = pwm_fan_get_active_panel(np);
	if (IS_ERR(panel)) {
		dev_warn(&pdev->dev, "No active panel, deferring probe");
		ret = -EPROBE_DEFER;
		goto err_exit;
	}
#endif /* CONFIG_QCOM_PANEL_EVENT_NOTIFIER */
#endif /* CONFIG_DRM */
	dev_dbg(&pdev->dev, "enter pwm fan probe\n");

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto err_exit;
	}

	ctx->vdd_supply = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(ctx->vdd_supply)) {
		ctx->vdd_supply = NULL;
		dev_dbg(&pdev->dev, "No vdd supply specified");
	}

#if IS_ENABLED(CONFIG_DRM)
	ctx->use_panel_notifiers = (count_panels(np, NULL) > 0);
#if !IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	ctx->active_panel = panel;
#endif
#endif

	ctx->wq = alloc_workqueue("fan_wq", WQ_UNBOUND, 1);
	if (!ctx->wq) {
		dev_err(&pdev->dev, "%s: could not create workqueue\n",
				__func__);
		ret = -ENOMEM;
		goto err_exit;
	}

	INIT_DELAYED_WORK(&ctx->fan_dwork, fan_work_func);
	INIT_WORK(&ctx->fan_recovery_work, fan_recovery_work_func);
	mutex_init(&ctx->lock);

	ret = platform_get_irq_byname(pdev, "fan_irq");
	if (ret < 0) {
		dev_err(&pdev->dev, "fan_irq not found: %d\n", ret);
		goto err_tach_gpio_dir;
	}
	ctx->irq = ret;

	ret = devm_request_irq(&pdev->dev, ctx->irq,
				  pwm_fan_irq_handler,
				  IRQF_TRIGGER_RISING,
				  "pwm-tach-fan", ctx);
	if (ret) {
		dev_err(&pdev->dev, "devm_request_irq failed\n");
		goto err_tach_gpio_dir;
	}

	/* Disable irq first & enable_irq in sys node */
	disable_irq(ctx->irq);
	reset_counters(ctx);

	now = ktime_get();
	ctx->last_disable_timestamp = now;
	ctx->last_stall_detect_timestamp = now;
	ctx->last_tach_timestamp = now;
	ctx->last_rpm_update_timestamp = now;

	ctx->pwm = devm_pwm_get(&pdev->dev, NULL);
	if (IS_ERR(ctx->pwm)) {
		ret = PTR_ERR(ctx->pwm);
		dev_err(&pdev->dev, "Could not get PWM %d\n", ret);
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
#if IS_ENABLED(CONFIG_DRM)
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	if (ctx->use_panel_notifiers) {
		uint32_t fan_id = 0;

		of_property_read_u32(pdev->dev.of_node, "reg", &fan_id);
		if (fan_id > 2) {
			dev_err(&pdev->dev, "Fan ID %d is unsupported\n", fan_id);
			ret = -EINVAL;
			goto err_tach_gpio_dir;
		}

		cookie = panel_event_notifier_register(
				PANEL_EVENT_NOTIFICATION_PRIMARY,
				fan_id == 2 ? PANEL_EVENT_NOTIFIER_CLIENT_FAN2 :
				(fan_id == 1 ? PANEL_EVENT_NOTIFIER_CLIENT_FAN1 : PANEL_EVENT_NOTIFIER_CLIENT_FAN0),
				NULL,
				&pwm_fan_panel_notifier_cb,
				&ctx->fb_notif);
		if (!cookie)
			goto err_tach_gpio_dir;
		ctx->notifier_cookie = cookie;
	}
#else /* !CONFIG_QCOM_PANEL_EVENT_NOTIFIER */
	if (ctx->use_panel_notifiers) {
		ctx->fb_notif.notifier_call = pwm_fan_fb_notifier_cb;
		ret = drm_panel_notifier_register(ctx->active_panel,
				&ctx->fb_notif);
		if (ret)
			goto err_tach_gpio_dir;
	}
#endif /* CONFIG_QCOM_PANEL_EVENT_NOTIFIER */
#endif /* CONFIG_DRM */
	ret = of_property_read_u32(pdev->dev.of_node, "oculus,vdd-to-pwm-delay-ms", &ctx->vdd_to_pwm_delay_ms);
	if (ret)
		ctx->vdd_to_pwm_delay_ms = DEFAULT_VDD_TO_PWM_DELAY_MS;

	ret = of_property_read_u32(pdev->dev.of_node, "oculus,rpm-per-sec", &ctx->rpm_per_sec);
	if (ret)
		ctx->rpm_per_sec = DEFAULT_RPM_PER_SEC;

	ret = of_property_read_u32(pdev->dev.of_node, "oculus,silent-rpm", &ctx->silent_rpm);
	if (ret)
		ctx->rpm_per_sec = DEFAULT_SILENT_RPM;

	ret = of_property_read_u32(pdev->dev.of_node, "oculus,min-pwm", &ctx->min_pwm);
	if (ret)
		ctx->min_pwm = DEFAULT_MIN_PWM;

	ret = of_property_read_u32(pdev->dev.of_node, "oculus,max-pwm", &ctx->max_pwm);
	if (ret)
		ctx->max_pwm = DEFAULT_MAX_PWM;

	ret = of_property_read_u32(pdev->dev.of_node, "oculus,cold-boot-pwm", &ctx->cold_boot_pwm);
	if (ret)
		ctx->cold_boot_pwm = COLD_BOOT_PWM;

	ret = of_property_read_u32(pdev->dev.of_node, "max-rpm", &ctx->max_rpm);
	if (ret) {
		dev_err(&pdev->dev, "Property 'max-rpm' cannot be read!\n");
		goto err_tach_gpio_dir;
	}

	ctx->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(ctx->pinctrl))
		goto err_tach_gpio_dir;

	ctx->active_state = pinctrl_lookup_state(ctx->pinctrl, "active");
	if (IS_ERR(ctx->active_state))
		ctx->active_state = NULL;

	ctx->idle_state = pinctrl_lookup_state(ctx->pinctrl, "idle");
	if (IS_ERR(ctx->idle_state))
		ctx->idle_state = NULL;

	ret = pwm_fan_of_get_cooling_data(&pdev->dev, ctx);
	if (ret)
		goto err_tach_gpio_dir;

	ret = of_property_read_u32(pdev->dev.of_node, "reg", &dt_addr);
	if (ret)
		goto err_tach_gpio_dir;

	// During probe, set the default state to 0 so that any non-zero vote is honored.
	ctx->pwm_fan_state = 0;

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
	}

	return 0;

err_tach_gpio_dir:
	if (!IS_ERR(ctx->pwm))
		pwm_disable(ctx->pwm);
err_exit:
#if IS_ENABLED(CONFIG_DRM)
	if (panels_np)
		of_node_put(panels_np);
#endif // CONFIG_DRM
	return ret;
}

static int pwm_fan_remove(struct platform_device *pdev)
{
	struct pwm_fan_ctx *ctx = platform_get_drvdata(pdev);

#if IS_ENABLED(CONFIG_DRM)
#if IS_ENABLED(CONFIG_QCOM_PANEL_EVENT_NOTIFIER)
	if (ctx->notifier_cookie)
		panel_event_notifier_unregister(&ctx->notifier_cookie);
#else
	if (ctx->active_panel)
		drm_panel_notifier_unregister(ctx->active_panel, &ctx->fb_notif);
#endif
#endif

	thermal_cooling_device_unregister(ctx->cdev);
	mutex_lock(&ctx->lock);
	if (ctx->pwm_value)
		disable_fan_locked(ctx);
	mutex_unlock(&ctx->lock);

	destroy_workqueue(ctx->wq);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int pwm_fan_suspend(struct device *dev)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int ret = 0;

	mutex_lock(&ctx->lock);

	/* Don't overwrite resume_rpm_value if already disabled */
	if (!pwm_is_enabled(ctx->pwm))
		goto end_pwm_fan_suspend;

	if (ctx->rpm_value > 0) {
		ctx->resume_rpm_value = ctx->rpm_value;

		ret = set_rpm_locked(ctx, 0, true);
	}

end_pwm_fan_suspend:
	mutex_unlock(&ctx->lock);
	return ret;
}

static int pwm_fan_resume(struct device *dev)
{
	struct pwm_fan_ctx *ctx = dev_get_drvdata(dev);
	int32_t resume_rpm;
	int ret = 0;

	mutex_lock(&ctx->lock);

	/* Limit fan speed while display is off */
	if (ctx->use_panel_notifiers && !ctx->is_display_on)
		resume_rpm = ctx->resume_rpm_value;
	else
		resume_rpm =  min(ctx->resume_rpm_value, ctx->silent_rpm);

	if (resume_rpm > 0)
		ret = set_rpm_locked(ctx, resume_rpm, false);

	mutex_unlock(&ctx->lock);
	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops pwm_fan_pm = {
	/*
	 * Use late suspend / early resume to avoid toggle fan off and back
	 * on upon a failed suspend attempt.
	 */
	SET_LATE_SYSTEM_SLEEP_PM_OPS(pwm_fan_suspend, pwm_fan_resume)
};

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
