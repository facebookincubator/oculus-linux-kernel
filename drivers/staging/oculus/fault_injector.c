// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>

#define FAULT_INJECTOR_TEST_DRIVER "fault_injector"

#define PHYMEM_VIOLATOR_SIZE 4

struct fault_injector_dev {
	void __iomem *base;
	struct device *dev;

};

static ssize_t trigger_phymem_violation_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t trigger_phymem_violation_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	uint32_t val = 0;
	struct fault_injector_dev *finjector_dev = dev_get_drvdata(dev);

	if (!finjector_dev || !finjector_dev->base)
		return -EINVAL;

	if (count < 1)
		return -EINVAL;

	if (kstrtouint(buf, 0, &val))
		return -EINVAL;

	if (val == 1)
		dev_info(dev, "Read protected phymem: 0x%X\n", ioread32(finjector_dev->base));
	else
		dev_err(dev, "Invalid input: %s\n", buf);

	return count;
}
static DEVICE_ATTR_RW(trigger_phymem_violation);

/* sysfs array with all driver files */
static struct attribute *fault_injector_attributes[] = {
	&dev_attr_trigger_phymem_violation.attr,
	NULL
};

/* sysfs attribute group */
static const struct attribute_group fault_injector_attr_group = {
	.attrs = fault_injector_attributes,
};

/* create sysfs interface */
static int fault_injector_sysfs_create(struct platform_device *pdev)
{
	return sysfs_create_group(&pdev->dev.kobj, &fault_injector_attr_group);
}

/* remove sysfs interface */
static void fault_injector_sysfs_remove(struct platform_device *pdev)
{
	sysfs_remove_group(&pdev->dev.kobj, &fault_injector_attr_group);
}

static int fault_injector_probe(struct platform_device *pdev)
{
	int result = 0;
	u32 base_addr;
	struct fault_injector_dev *finjector_dev;

	finjector_dev = devm_kzalloc(&pdev->dev, sizeof(*finjector_dev), GFP_KERNEL);
	if (!finjector_dev)
		return -ENOMEM;

	result = of_property_read_u32(pdev->dev.of_node, "phymem-violator-base-addr", &base_addr);
	if (result < 0) {
		dev_err(&pdev->dev, "Failed to read phymem-violator-base-addr property: %d\n", result);
		return result;
	}

	finjector_dev->base = ioremap(base_addr, PHYMEM_VIOLATOR_SIZE);
	if (!finjector_dev->base) {
		dev_err(&pdev->dev, "Failed to ioremap physical address: 0x%X\n", base_addr);
		return -ENOMEM;
	}

	finjector_dev->dev = &pdev->dev;
	dev_set_drvdata(&pdev->dev, finjector_dev);

	fault_injector_sysfs_create(pdev);

	dev_dbg(&pdev->dev, "fault_injector driver initialized\n");
	return 0;
}

static int fault_injector_remove(struct platform_device *pdev)
{
	struct fault_injector_dev *finjector_dev = platform_get_drvdata(pdev);

	fault_injector_sysfs_remove(pdev);

	if (finjector_dev && finjector_dev->base)
		iounmap(finjector_dev->base);
	return 0;
}

/* Driver Info */
static const struct of_device_id fault_injector_match_table[] = {
	{ .compatible = "meta,fault_injector" },
	{ },
};

static struct platform_driver fault_injector_platform_driver = {
	.driver = {
		.name = "meta,fault_injector",
		.of_match_table = fault_injector_match_table,
		.owner = THIS_MODULE,
	},
	.probe = fault_injector_probe,
	.remove = fault_injector_remove,
};

static int __init fault_injector_init(void)
{
	platform_driver_register(&fault_injector_platform_driver);
	return 0;
}

static void __exit fault_injector_exit(void)
{
	platform_driver_unregister(&fault_injector_platform_driver);
}

module_init(fault_injector_init);
module_exit(fault_injector_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("fault injector Test");
