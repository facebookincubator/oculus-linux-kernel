#include <linux/delay.h>
#include <linux/device.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>


struct blu_pd {
	/* platform device handle */
	struct device *dev;
	struct gpio_desc *boot_gpio, *reset_gpio;
  struct mutex lock;
};

static ssize_t bootload_show(struct device *dev, struct device_attribute *attr, char *buf) {
	return scnprintf(buf, PAGE_SIZE, "N/A\n");
}

static ssize_t bootload_store(struct device *dev, struct device_attribute *attr,
    const char *buf, size_t count) {
	struct blu_pd *bpd;
  long val;
  int rc;

	bpd = (struct blu_pd*) dev_get_drvdata(dev);
	if (!bpd) {
	  return -ENOMEM;
	}

  rc = kstrtol(buf, 10, &val);
  if (rc != 0 || val <= 0) {
		dev_err(bpd->dev, "NOP for value (%s): %d", buf, rc);
		return rc;
  }

	rc = mutex_lock_interruptible(&bpd->lock);

	if (rc != 0) {
		dev_err(bpd->dev, "Failed to get mutex: %d", rc);
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

	mutex_unlock(&bpd->lock);

	return count;
}

static DEVICE_ATTR(bootload, 0644, bootload_show, bootload_store);

static struct attribute *blu_attrs[] = {
	&dev_attr_bootload.attr,
	NULL
};

ATTRIBUTE_GROUPS(blu);

static int blu_probe(struct platform_device *pdev) {
	struct blu_pd *bpd;
	int rc = 0;

  bpd = devm_kzalloc(&pdev->dev, sizeof(*bpd), GFP_KERNEL);
  if (!bpd) {
		dev_err(&pdev->dev, "Failed to allocate device: %ld\n", PTR_ERR(bpd));
    return -ENOMEM;
  }

  if (IS_ERR(bpd->boot_gpio = devm_gpiod_get(&pdev->dev, "boot", GPIOD_OUT_LOW))) {
		dev_err(&pdev->dev, "devm_gpiod_get 'boot' failed: %ld\n", PTR_ERR(bpd->boot_gpio));
		return PTR_ERR(bpd->boot_gpio);
  }

  if (IS_ERR(bpd->reset_gpio = devm_gpiod_get(&pdev->dev, "reset", GPIOD_OUT_LOW))) {
		dev_err(&pdev->dev, "devm_gpiod_get 'reset' failed: %ld\n", PTR_ERR(bpd->reset_gpio));
		return PTR_ERR(bpd->reset_gpio);
  }

  bpd->dev = &pdev->dev;
  dev_set_drvdata(&pdev->dev, bpd);

	rc = sysfs_create_groups(&bpd->dev->kobj, blu_groups);
	if (rc) {
    dev_err(bpd->dev, "Could not create blu_fw sysfs, error: %d\n", rc);
    return rc;
  }

	mutex_init(&bpd->lock);

  return rc;
}

static int blu_remove(struct platform_device *pdev) {
	struct blu_pd* bpd = (struct blu_pd*) platform_get_drvdata(pdev);

	sysfs_remove_groups(&bpd->dev->kobj, blu_groups);
  mutex_destroy(&bpd->lock);
  return 0;
}

/* Driver Info */
static const struct of_device_id blu_match_table[] = {
		{ .compatible = "oculus,blu", },
		{ },
};

static struct platform_driver blu_driver = {
	.driver = {
		.name = "oculus,blu",
		.of_match_table = blu_match_table,
		.owner = THIS_MODULE,
	},
	.probe	= blu_probe,
	.remove = blu_remove,
};

static int __init blu_fw_init(void) {
  int rc = 0;

	rc = platform_driver_register(&blu_driver);
	if (rc) {
    pr_err("Could not register blu_fw device, error: %d\n", rc);
    return rc;
  }

  return rc;
}

static void __exit blu_fw_exit(void) {
	platform_driver_unregister(&blu_driver);
}

/* Register module functions */
module_init(blu_fw_init);
module_exit(blu_fw_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("maksymowych@fb.com");
