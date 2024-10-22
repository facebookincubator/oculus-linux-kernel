/* SPDX-License-Identifier: GPL-2.0 */

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

#define AW8646_MAX_FREQUENCY			250000
#define AW8646_DEFAULT_FREQUENCY		1000

struct aw8646 {
	struct mutex lock;
	struct device *dev;
	struct hrtimer hrtimer;
	struct platform_device *pdev;

	ktime_t kinterval;
	uint32_t step_num;
	uint32_t timer_cnt;
	uint32_t half_period;
	uint32_t step_frequency;

	int dir_pin;		/* direction input pin */
	int dir_pin_state;
	int en_pin;		/* driver enable control input pin */
	int fault_pin;
	int fault_pin_state;
	int fault_indication;
	int step_pin;		/* step input pin */
	int sleep_pin;		/* sleep mode input pin */
};

#define DRIVER_NAME				"aw8646_motor"

#define AW_LOGI(format, ...) \
	pr_info("[%s][%04d]%s: " format "\n", DRIVER_NAME, __LINE__, __func__, ##__VA_ARGS__)
#define AW_LOGE(format, ...) \
	pr_err("[%s][%04d]%s: " format "\n", DRIVER_NAME, __LINE__, __func__, ##__VA_ARGS__)

enum aw_fault_state {
	AW_FAULT_NO = 0,
	AW_FAULT_YES = -1,
};

enum aw_gpio_state {
	AW_GPIO_LOW = 0,
	AW_GPIO_HIGH = 1,
};

static int aw8646_parse_dts(struct aw8646 *aw8646)
{
	aw8646->en_pin = of_get_named_gpio(aw8646->dev->of_node, "en-gpio", 0);
	if (!gpio_is_valid(aw8646->en_pin))
		return -EINVAL;

	aw8646->dir_pin = of_get_named_gpio(aw8646->dev->of_node, "dir-gpio", 0);
	if (!gpio_is_valid(aw8646->dir_pin))
		return -EINVAL;

	aw8646->step_pin = of_get_named_gpio(aw8646->dev->of_node, "step-gpio", 0);
	if (!gpio_is_valid(aw8646->step_pin))
		return -EINVAL;

	aw8646->sleep_pin = of_get_named_gpio(aw8646->dev->of_node, "sleep-gpio", 0);
	if (!gpio_is_valid(aw8646->sleep_pin))
		return -EINVAL;

	aw8646->fault_pin = of_get_named_gpio(aw8646->dev->of_node, "fault-gpio", 0);
	if (!gpio_is_valid(aw8646->fault_pin))
		return -EINVAL;

	AW_LOGI("aw8646->en_pin %d", aw8646->en_pin);
	AW_LOGI("aw8646->dir_pin %d", aw8646->dir_pin);
	AW_LOGI("aw8646->step_pin %d", aw8646->step_pin);
	AW_LOGI("aw8646->sleep_pin %d", aw8646->sleep_pin);
	AW_LOGI("aw8646->fault_pin %d", aw8646->fault_pin);

	return 0;
}

static int aw8646_gpio_request(struct aw8646 *aw8646)
{
	int ret;

	ret = devm_gpio_request_one(aw8646->dev, aw8646->en_pin, GPIOF_OUT_INIT_HIGH,
			"aw8646_en");
	if (ret) {
		AW_LOGE("failed to request en pin");
		return ret;
	}

	ret = devm_gpio_request_one(aw8646->dev, aw8646->dir_pin, GPIOF_OUT_INIT_LOW,
			"aw8646_dir");
	if (ret) {
		AW_LOGE("failed to request dir pin");
		return ret;
	}

	ret = devm_gpio_request_one(aw8646->dev, aw8646->step_pin, GPIOF_OUT_INIT_LOW,
			"aw8646_step");
	if (ret) {
		AW_LOGE("failed to request step pin");
		return ret;
	}

	ret = devm_gpio_request_one(aw8646->dev, aw8646->sleep_pin, GPIOF_OUT_INIT_HIGH,
			"aw8646_sleep");
	if (ret) {
		AW_LOGE("failed to request sleep pin");
		return ret;
	}

	ret = devm_gpio_request_one(aw8646->dev, aw8646->fault_pin, GPIOF_IN,
			"aw8646_fault");
	if (ret) {
		AW_LOGE("failed to request fault pin");
		return ret;
	}

	return ret;
}

static enum hrtimer_restart aw8646_motor_timer_func(struct hrtimer *hrtimer)
{
	int ret = 0;
	struct aw8646 *aw8646 = container_of(hrtimer, struct aw8646, hrtimer);

	if (aw8646->timer_cnt < aw8646->step_num) {
		aw8646->timer_cnt++;
		gpio_set_value_cansleep(aw8646->step_pin, AW_GPIO_HIGH);
		udelay(aw8646->half_period);
		gpio_set_value_cansleep(aw8646->step_pin, AW_GPIO_LOW);
		hrtimer_forward_now(&aw8646->hrtimer, aw8646->kinterval);
		ret = HRTIMER_RESTART;
	} else {
		gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
		AW_LOGI("play end, %u steps completed", aw8646->timer_cnt);
		ret = HRTIMER_NORESTART;
	}

	return ret;
}

static ssize_t fault_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	val = gpio_get_value_cansleep(aw8646->fault_pin);

	if (val > 0) {
		val = aw8646->fault_indication = AW_FAULT_NO;
		AW_LOGI("No fault");
	} else {
		val = aw8646->fault_indication = AW_FAULT_YES;
		AW_LOGI("Fault!");
	}

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", val);

	return len;
}
static DEVICE_ATTR_RO(fault);

static ssize_t sleep_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	val = gpio_get_value_cansleep(aw8646->sleep_pin);
	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", !val);

	return len;
}

static ssize_t sleep_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	int val = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&aw8646->lock);
	if (val > 0) {
		hrtimer_cancel(&aw8646->hrtimer);
		gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_LOW);
		gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
		AW_LOGI("set the chip to sleep mode");
	} else {
		gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_HIGH);
		AW_LOGI("set the chip to no sleep mode");
	}
	mutex_unlock(&aw8646->lock);

	return count;
}
static DEVICE_ATTR_RW(sleep);

static ssize_t direction_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int val = 0;
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	val = gpio_get_value_cansleep(aw8646->dir_pin);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", val);

	return len;
}

static ssize_t direction_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	int val = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	if (kstrtoint(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&aw8646->lock);
	if (val > 0) {
		aw8646->dir_pin_state = AW_GPIO_HIGH;
		AW_LOGI("set the dir pin of the chip to high");
	} else {
		aw8646->dir_pin_state = AW_GPIO_LOW;
		AW_LOGI("set the dir pin of the chip to low");
	}
	mutex_unlock(&aw8646->lock);

	return count;
}
static DEVICE_ATTR_RW(direction);

static ssize_t step_frequency_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw8646->step_frequency);

	return len;
}

static ssize_t step_frequency_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	uint32_t val = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	mutex_lock(&aw8646->lock);
	if (val > 0 && val <= AW8646_MAX_FREQUENCY) {
		aw8646->step_frequency = val;
		AW_LOGI("set the step frequency to %u", aw8646->step_frequency);
	} else {
		AW_LOGE("wrong frequency parameter: %u", val);
	}
	mutex_unlock(&aw8646->lock);

	return count;
}
static DEVICE_ATTR_RW(step_frequency);

static ssize_t steps_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw8646->timer_cnt);

	return len;
}
static DEVICE_ATTR_RO(steps);

static ssize_t activate_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len, "%u\n", aw8646->step_num);

	return len;
}

static ssize_t activate_store(struct device *dev, struct device_attribute *attr, const char *buf,
		size_t count)
{
	uint32_t steps = 0;
	struct aw8646 *aw8646 = dev_get_drvdata(dev);

	if (kstrtouint(buf, 0, &steps))
		return -EINVAL;

	AW_LOGI("enter, input steps = %u", steps);

	mutex_lock(&aw8646->lock);
	if (steps > 0) {
		/* Stop the currently playing */
		hrtimer_cancel(&aw8646->hrtimer);
		gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
		AW_LOGI("stop play, %u steps completed", aw8646->timer_cnt);
		aw8646->timer_cnt = 0;

		aw8646->step_num = steps;
		aw8646->half_period = 500000 / aw8646->step_frequency;
		aw8646->kinterval = ktime_set(0, (aw8646->half_period * 1000));

		AW_LOGI("start play, steps = %u, dir = %d", aw8646->step_num,
				aw8646->dir_pin_state);
		gpio_set_value_cansleep(aw8646->dir_pin, aw8646->dir_pin_state);
		gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_LOW);
		hrtimer_start(&aw8646->hrtimer, ktime_set(0, 1000), HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&aw8646->hrtimer);
		gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
		AW_LOGI("stop play, %u steps completed", aw8646->timer_cnt);
	}
	mutex_unlock(&aw8646->lock);

	return count;
}
static DEVICE_ATTR_RW(activate);

static struct attribute *aw8646_attributes[] = {
	&dev_attr_fault.attr,
	&dev_attr_sleep.attr,
	&dev_attr_direction.attr,
	&dev_attr_step_frequency.attr,
	&dev_attr_steps.attr,
	&dev_attr_activate.attr,
	NULL,
};

struct attribute_group aw8646_attribute_group = {
	.name = DRIVER_NAME,
	.attrs = aw8646_attributes,
};

static int aw8646_probe(struct platform_device *pdev)
{
	struct aw8646 *aw8646 = NULL;

	aw8646 = devm_kzalloc(&pdev->dev, sizeof(struct aw8646), GFP_KERNEL);
	if (!aw8646)
		return -ENOMEM;

	aw8646->pdev = pdev;
	aw8646->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, aw8646);

	if (aw8646_parse_dts(aw8646)) {
		AW_LOGE("failed to parse aw8646 dts");
		return -ERANGE;
	}

	if (aw8646_gpio_request(aw8646)) {
		AW_LOGE("failed to request aw8646 gpio");
		return -ERANGE;
	}

	if (sysfs_create_group(&aw8646->dev->kobj, &aw8646_attribute_group)) {
		AW_LOGE("failed to create sysfs group");
		return -ERANGE;
	}

	mutex_init(&aw8646->lock);
	hrtimer_init(&aw8646->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aw8646->hrtimer.function = aw8646_motor_timer_func;

	aw8646->step_frequency = AW8646_DEFAULT_FREQUENCY;

	AW_LOGI("aw8646 motor driver probe successfully");

	return 0;
}

static int aw8646_remove(struct platform_device *pdev)
{
	struct aw8646 *aw8646 = dev_get_drvdata(&pdev->dev);

	AW_LOGI("aw8646 motor driver remove");

	hrtimer_cancel(&aw8646->hrtimer);
	gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_LOW);
	gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
	mutex_destroy(&aw8646->lock);
	sysfs_remove_group(&aw8646->dev->kobj, &aw8646_attribute_group);

	return 0;
}

static int aw8646_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct aw8646 *aw8646 = dev_get_drvdata(&pdev->dev);

	mutex_lock(&aw8646->lock);
	hrtimer_cancel(&aw8646->hrtimer);
	gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_LOW);
	gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
	AW_LOGI("set the chip to sleep mode");
	mutex_unlock(&aw8646->lock);

	return 0;
}

static int aw8646_resume(struct platform_device *pdev)
{
	struct aw8646 *aw8646 = dev_get_drvdata(&pdev->dev);

	mutex_lock(&aw8646->lock);
	gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_HIGH);
	AW_LOGI("set the chip to no sleep mode");
	mutex_unlock(&aw8646->lock);

	return 0;
}

static void aw8646_shutdown(struct platform_device *pdev)
{
	struct aw8646 *aw8646 = dev_get_drvdata(&pdev->dev);

	mutex_lock(&aw8646->lock);
	hrtimer_cancel(&aw8646->hrtimer);
	gpio_set_value_cansleep(aw8646->sleep_pin, AW_GPIO_LOW);
	gpio_set_value_cansleep(aw8646->en_pin, AW_GPIO_HIGH);
	AW_LOGI("set the chip to sleep mode");
	mutex_unlock(&aw8646->lock);
}

const struct of_device_id aw8646_dt_match[] = {
	{.compatible = "awinic,aw8646_motor"},
	{},
};
MODULE_DEVICE_TABLE(of, aw8646_dt_match);

static struct platform_driver aw8646_driver = {
	.driver = {
		.name	= "aw8646_motor",
		.of_match_table	= aw8646_dt_match,
	},
	.probe		= aw8646_probe,
	.remove		= aw8646_remove,
	.suspend	= aw8646_suspend,
	.resume		= aw8646_resume,
	.shutdown	= aw8646_shutdown,
};
module_platform_driver(aw8646_driver);

MODULE_DESCRIPTION("aw8646 motor Driver");
MODULE_LICENSE("GPL v2");
