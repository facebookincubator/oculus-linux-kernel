// SPDX-License-Identifier: GPL-2.0

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>

static int eye_face_led_probe(struct platform_device *pdev)
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

static int eye_face_led_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id eye_face_led_of_match[] = {
	{ .compatible = "oculus,eye_face_led", },
	{}
};

MODULE_DEVICE_TABLE(of, eye_face_led_of_match);

/* Driver Info */
static struct platform_driver eye_face_led_driver = {
	.driver = {
		.name = "oculus,eye_face_led",
		.owner = THIS_MODULE,
		.of_match_table = eye_face_led_of_match,
	},
	.probe	= eye_face_led_probe,
	.remove = eye_face_led_remove,
};

static int __init eye_face_led_init(void)
{
	platform_driver_register(&eye_face_led_driver);

	return 0;
}

static void __exit eye_face_led_exit(void)
{
	platform_driver_unregister(&eye_face_led_driver);
}

module_init(eye_face_led_init);
module_exit(eye_face_led_exit);

MODULE_AUTHOR("Rohit Vijayaraghavan <rohitv@fb.com>");
MODULE_DESCRIPTION("Eye Face Led driver");
MODULE_LICENSE("GPL v2");
