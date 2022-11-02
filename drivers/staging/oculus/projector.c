// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>

static int projector_probe(struct platform_device *pdev)
{
	struct pinctrl *pinctrl = NULL;
	struct pinctrl_state *pins_default = NULL;
	int result = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(&pdev->dev, "Failed to get pin ctrl");
		return -EINVAL;
	}

	pins_default = pinctrl_lookup_state(pinctrl, "default");
	if (IS_ERR_OR_NULL(pins_default)) {
		dev_err(&pdev->dev, "Failed to lookup pinctrl default state");
		return -EINVAL;
	}

	result = pinctrl_select_state(pinctrl, pins_default);
	if (result != 0)
		dev_err(&pdev->dev, "Failed to set pin state");

	return result;
}

static int projector_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "Projector remove\n");
	return 0;
}

static const struct of_device_id projector_of_match[] = {
	{ .compatible = "oculus,projector", },
	{}
};

MODULE_DEVICE_TABLE(of, projector_of_match);

/*Driver Info */
static struct platform_driver projector_driver = {
	.driver = {
		.name = "oculus,projector",
		.owner = THIS_MODULE,
		.of_match_table = projector_of_match,
	},
	.probe	= projector_probe,
	.remove = projector_remove,
};

static int __init projector_init(void)
{
	platform_driver_register(&projector_driver);
	return 0;
}

static void __exit projector_exit(void)
{
	platform_driver_unregister(&projector_driver);
}

module_init(projector_init);
module_exit(projector_exit);

MODULE_AUTHOR("Suresh Vakkalanka <sureshv@fb.com>");
MODULE_DESCRIPTION("Depth Projector driver");
MODULE_LICENSE("GPL v2");
