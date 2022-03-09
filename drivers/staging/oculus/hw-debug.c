#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>

struct hw_debug_ctx {
	struct device *dev;
	unsigned int power_marker_gpio_0;
	unsigned int power_marker_gpio_1;
};

#define POWER_MARKER_GPIO_0_DT_PROP_NAME "power-marker-gpio-0"
#define POWER_MARKER_GPIO_1_DT_PROP_NAME "power-marker-gpio-1"
#define POWER_MARKER_GPIO_0_EXPORT_NAME "power_marker_gpio_0"
#define POWER_MARKER_GPIO_1_EXPORT_NAME "power_marker_gpio_1"

static int export_and_link_gpio(
	struct device *dev,
	unsigned int gpio,
	const char *link_name)
{
	int rc = 0;

	if (!gpio_is_valid(gpio)) {
		dev_err(dev, "gpio %u is not valid", gpio);
		return -EINVAL;
	}

	rc = devm_gpio_request(dev, gpio, link_name);
	if (rc) {
		dev_err(dev, "failed to request gpio %u", gpio);
		return rc;
	}

	rc = gpio_export(gpio, true);
	if (rc) {
		dev_err(dev, "failed to export gpio %u", gpio);
		return rc;
	}

	rc = gpio_export_link(dev, link_name, gpio);
	if (rc) {
		dev_err(dev, "failed to export gpio %u as link %s",
			gpio, link_name);
		return rc;
	}

	return rc;
}

static int hw_debug_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hw_debug_ctx *ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	int rc = 0;

	if (!ctx)
		return -ENOMEM;

	ctx->dev = dev;

	// Export GPIOs for power measurements

	ctx->power_marker_gpio_0 =
		of_get_named_gpio(dev->of_node,
				  POWER_MARKER_GPIO_0_DT_PROP_NAME,
				  0);
	if (ctx->power_marker_gpio_0 < 0) {
		rc = ctx->power_marker_gpio_0;
		dev_err(dev, "failed to get gpio named %s, rc = %d",
			POWER_MARKER_GPIO_0_DT_PROP_NAME, rc);
		return rc;
	}

	ctx->power_marker_gpio_1 = of_get_named_gpio(dev->of_node,
		POWER_MARKER_GPIO_1_DT_PROP_NAME, 0);
	if (ctx->power_marker_gpio_1 < 0) {
		rc = ctx->power_marker_gpio_1;
		dev_err(dev, "failed to get gpio named %s, rc = %d",
			POWER_MARKER_GPIO_1_DT_PROP_NAME, rc);
		return rc;
	}

	rc = export_and_link_gpio(dev, ctx->power_marker_gpio_0,
				  POWER_MARKER_GPIO_0_EXPORT_NAME);
	if (rc) {
		dev_err(dev,
			"failed to export and link power marker gpio 0, rc = %d",
			rc);
		return rc;
	}

	rc = export_and_link_gpio(dev, ctx->power_marker_gpio_1,
				  POWER_MARKER_GPIO_1_EXPORT_NAME);
	if (rc) {
		dev_err(dev,
			"failed to export and link power marker gpio 1, rc = %d",
			rc);
		return rc;
	}

	platform_set_drvdata(pdev, ctx);

	dev_info(dev, "hw-debug probe success.\n");

	return rc;
}

static int hw_debug_remove(struct platform_device *pdev)
{
	struct hw_debug_ctx *ctx = platform_get_drvdata(pdev);

	devm_kfree(ctx->dev, ctx);

	return 0;
}

static const struct of_device_id hw_debug_of_match[] = {
	{
		.compatible = "oculus,hw-debug",
	},
	{},
};

static struct platform_driver hw_debug_driver = {
	.driver = {
		.name = "hw-debug",
		.of_match_table = hw_debug_of_match,
	},
	.probe = hw_debug_probe,
	.remove = hw_debug_remove,
};

static int __init hw_debug_driver_init(void)
{
	int rc = 0;

	rc = platform_driver_register(&hw_debug_driver);
	if (rc) {
		pr_err("Unable to register Oculus HW debug driver: %d\n", rc);
		return rc;
	}

	return rc;
}

static void __exit hw_debug_driver_exit(void)
{
	platform_driver_unregister(&hw_debug_driver);
}

module_init(hw_debug_driver_init);
module_exit(hw_debug_driver_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION(
	"Platform driver for exposing various Oculus specific debug capabilities");
