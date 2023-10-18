#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/sysfs.h>


struct blu_pd {
	/* platform device handle */
	struct device *dev;
	struct gpio_desc *boot_gpio, *reset_gpio;
	struct regulator *vdd_vreg;
	struct regulator *uart_vreg;
	struct pinctrl *pinctrl;
	struct pinctrl_state *active_state;
	struct pinctrl_state *sleep_state;
	struct mutex lock;
};

static bool store_is_true(struct device *dev, const char *buf, size_t count)
{
	int rc;
	bool should_run;

	if (count == 0) {
		dev_err(dev, "NOP on empty buffer");
		return false;
	}

	rc = kstrtobool(buf, &should_run);
	if (rc != 0) {
		dev_err(dev, "NOP for value (%s): %d", buf, rc);
		return false;
	}

	return should_run;
}

static ssize_t bootload_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "N/A\n");
}

static ssize_t bootload_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct blu_pd *bpd;
	int rc;

	bpd = dev_get_drvdata(dev);
	if (!bpd)
		return -ENOMEM;

	if (!store_is_true(bpd->dev, buf, count)) {
		dev_err(bpd->dev, "NOP on invalid store trigger");
		return -EINVAL;
	}

	rc = mutex_lock_interruptible(&bpd->lock);
	if (rc != 0) {
		dev_warn(dev, "%s aborted due to signal. status=%d", __func__, rc);
		return rc;
	}

	// Export GPIOs and put MCU in bootloader mode
	gpiod_set_value(bpd->reset_gpio, 1);
	msleep(100);
	gpiod_set_value(bpd->boot_gpio, 1);
	msleep(100);
	gpiod_set_value(bpd->reset_gpio, 0);
	msleep(100);
	gpiod_set_value(bpd->boot_gpio, 0);
	msleep(100);

	mutex_unlock(&bpd->lock);

	return count;
}

static ssize_t reset_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "N/A\n");
}

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct blu_pd *bpd;
	int rc;

	bpd = dev_get_drvdata(dev);
	if (!bpd)
		return -ENOMEM;

	if (!store_is_true(bpd->dev, buf, count)) {
		dev_err(bpd->dev, "NOP on invalid store trigger");
		return -EINVAL;
	}

	rc = mutex_lock_interruptible(&bpd->lock);
	if (rc != 0) {
		dev_err(bpd->dev, "Failed to get mutex: %d", rc);
		return rc;
	}

	// Pin reset MCU
	gpiod_set_value(bpd->reset_gpio, 1);
	msleep(100);
	gpiod_set_value(bpd->reset_gpio, 0);
	msleep(100);
	mutex_unlock(&bpd->lock);

	return count;
}

static DEVICE_ATTR(bootload, 0644, bootload_show, bootload_store);
static DEVICE_ATTR(reset, 0644, reset_show, reset_store);

static struct attribute *blu_attrs[] = {
	&dev_attr_bootload.attr,
	&dev_attr_reset.attr,
	NULL
};

ATTRIBUTE_GROUPS(blu);

static int blu_pm_resume(struct device *dev)
{
	struct blu_pd *bpd = dev_get_drvdata(dev);
	int rc;

	/*
	 * T148253155: We must enable the MCU VDD and level shifter regulators
	 * before enabling the UART. Return early with the error on failure.
	 */
	if (!IS_ERR_OR_NULL(bpd->vdd_vreg)) {
		rc = regulator_enable(bpd->vdd_vreg);
		if (rc)
			return rc;
	}

	if (!IS_ERR_OR_NULL(bpd->uart_vreg)) {
		rc = regulator_enable(bpd->uart_vreg);
		if (rc) {
			if (!IS_ERR_OR_NULL(bpd->vdd_vreg))
				regulator_disable(bpd->vdd_vreg);
			return rc;
		}
	}

	if (!IS_ERR_OR_NULL(bpd->active_state))
		rc = pinctrl_select_state(bpd->pinctrl, bpd->active_state);

	return rc;
}

static int blu_probe(struct platform_device *pdev)
{
	struct blu_pd *bpd;
	int rc = 0;

	bpd = devm_kzalloc(&pdev->dev, sizeof(*bpd), GFP_KERNEL);
	if (!bpd)
		return -ENOMEM;

	bpd->boot_gpio = devm_gpiod_get(&pdev->dev, "boot", GPIOD_OUT_LOW);
	if (IS_ERR(bpd->boot_gpio)) {
		dev_err(&pdev->dev, "devm_gpiod_get 'boot' failed: %ld\n", PTR_ERR(bpd->boot_gpio));
		return PTR_ERR(bpd->boot_gpio);
	}

	bpd->reset_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(bpd->reset_gpio)) {
		dev_err(&pdev->dev, "devm_gpiod_get 'reset' failed: %ld\n", PTR_ERR(bpd->reset_gpio));
		return PTR_ERR(bpd->reset_gpio);
	}

	bpd->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, bpd);

	/* Configure the BLU UART power management if applicable. */
	bpd->pinctrl = devm_pinctrl_get(bpd->dev);
	if (IS_ERR_OR_NULL(bpd->pinctrl))
		goto no_pinctrl;

	bpd->vdd_vreg = devm_regulator_get(bpd->dev, "vdd");
	if (IS_ERR_OR_NULL(bpd->vdd_vreg))
		goto no_pinctrl;

	bpd->uart_vreg = devm_regulator_get(bpd->dev, "uart");
	if (IS_ERR_OR_NULL(bpd->uart_vreg))
		goto no_pinctrl;

	bpd->active_state = pinctrl_lookup_state(bpd->pinctrl, "uart_active");
	if (IS_ERR_OR_NULL(bpd->active_state))
		goto no_pinctrl;

	bpd->sleep_state = pinctrl_lookup_state(bpd->pinctrl, "uart_sleep");
	if (IS_ERR(bpd->sleep_state)) {
		/* If we have an active state we require a sleep state as well. */
		dev_err(bpd->dev, "pinctrl_lookup_state 'uart_sleep' failed: %ld\n",
				PTR_ERR(bpd->sleep_state));
		return PTR_ERR(bpd->sleep_state);
	}

	rc = blu_pm_resume(bpd->dev);
	if (rc) {
		dev_err(bpd->dev, "pinctrl_select_state 'uart_active' failed: %d\n", rc);
		return rc;
	}

no_pinctrl:
	rc = sysfs_create_groups(&bpd->dev->kobj, blu_groups);
	if (rc) {
		dev_err(bpd->dev, "Could not create blu_fw sysfs, error: %d\n", rc);
		return rc;
	}

	mutex_init(&bpd->lock);

	return rc;
}

static int blu_pm_suspend(struct device *dev)
{
	struct blu_pd *bpd = dev_get_drvdata(dev);
	int rc;

	/*
	 * T148253155: Shut down the UART before disabling the level shifter
	 * and MCU VDD regulators.
	 */
	if (!IS_ERR_OR_NULL(bpd->sleep_state)) {
		rc = pinctrl_select_state(bpd->pinctrl, bpd->sleep_state);
		if (rc) {
			dev_err(dev, "pinctrl_select_state 'uart_sleep' failed: %d\n", rc);
			return rc;
		}
	}

	if (!IS_ERR_OR_NULL(bpd->uart_vreg))
		regulator_disable(bpd->uart_vreg);

	if (!IS_ERR_OR_NULL(bpd->vdd_vreg))
		regulator_disable(bpd->vdd_vreg);

	return 0;
}

static int blu_remove(struct platform_device *pdev)
{
	struct blu_pd *bpd = platform_get_drvdata(pdev);

	/* Select the UART sleep state and disable the regulators. */
	blu_pm_suspend(bpd->dev);

	sysfs_remove_groups(&bpd->dev->kobj, blu_groups);
	mutex_destroy(&bpd->lock);
	return 0;
}

/* Driver Info */
static const struct of_device_id blu_match_table[] = {
	{ .compatible = "oculus,blu", },
	{},
};

static SIMPLE_DEV_PM_OPS(blu_pm_ops, blu_pm_suspend, blu_pm_resume);

static struct platform_driver blu_driver = {
	.driver = {
		.name = "oculus,blu",
		.of_match_table = blu_match_table,
		.owner = THIS_MODULE,
		.pm = &blu_pm_ops,
	},
	.probe	= blu_probe,
	.remove = blu_remove,
};

static int __init blu_fw_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&blu_driver);
	if (rc) {
		pr_err("Could not register blu_fw device, error: %d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit blu_fw_exit(void)
{
	platform_driver_unregister(&blu_driver);
}

/* Register module functions */
module_init(blu_fw_init);
module_exit(blu_fw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("maksymowych@fb.com");
