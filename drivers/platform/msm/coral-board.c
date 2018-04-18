#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pwm.h>
#include <linux/spmi.h>
#include <linux/string.h>

#define FAN_DEFAULT_PERIOD_NS 40 * 1000
#define FAN_DEFAULT_DUTY_NS 30 * 1000
#define FAN_OFF_DUTY_NS 40 * 10000
#define FAN_MIN_SPEED_DUTY_NS 30 * 1000
#define FAN_MED_SPEED_DUTY_NS 8 * 1000
#define FAN_OFF_SPEED_LEVEL 0
#define FAN_MIN_SPEED_LEVEL 1
#define FAN_MED_SPEED_LEVEL 2
#define FAN_HIGH_SPEED_LEVEL 3
#define FAN_MAX_SPEED_LEVEL 4

struct coral_fan_ctx {
	struct device *dev;
	struct spmi_device *spmi;
	struct pwm_device *pwm;
	struct led_classdev fan_led_cdev;
	u16 fan_en_gpio;
	u32 speed_level;
	u8 auto_speed_level;
};

static ssize_t fan_adjust_speed_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	ssize_t rc;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct coral_fan_ctx *ctx =
	    container_of(led_cdev, struct coral_fan_ctx, fan_led_cdev);

	rc = sprintf(buf, "%u\n", ctx->speed_level);

	return rc;
}

static ssize_t fan_adjust_speed(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	u8 reg = 0;
	ssize_t rc = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct coral_fan_ctx *ctx =
	    container_of(led_cdev, struct coral_fan_ctx, fan_led_cdev);

	rc = kstrtou32(buf, 10, &ctx->speed_level);
	if (rc) {
		return len;
	}

	reg = 0x13;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB141, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}
	reg = 0x23;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB142, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}
	switch (ctx->speed_level) {
	case FAN_OFF_SPEED_LEVEL:
		reg = 0x00;
		break;
	case FAN_MIN_SPEED_LEVEL:
		reg = 0x0A;
		break;
	case FAN_MED_SPEED_LEVEL:
		reg = 0x19;
		break;
	case FAN_HIGH_SPEED_LEVEL:
		reg = 0xFF;
		break;
	default:
		/* Default to high if value > FAN_HIGH_SPEED_LEVEL */
		reg = 0xFF;
		break;
	}
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB144, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}
	reg = 0x80;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xC346, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}

	return len;
}

static ssize_t fan_adjust_auto_speed_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	ssize_t rc;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct coral_fan_ctx *ctx =
	    container_of(led_cdev, struct coral_fan_ctx, fan_led_cdev);

	rc = sprintf(buf, "%u\n", ctx->auto_speed_level);

	return rc;
}

static ssize_t fan_adjust_auto_speed(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t len)
{
	u8 reg = 0;
	ssize_t rc = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct coral_fan_ctx *ctx =
	    container_of(led_cdev, struct coral_fan_ctx, fan_led_cdev);

	rc = kstrtou8(buf, 10, &ctx->auto_speed_level);
	if (rc) {
		return len;
	}

	reg = 0x13;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB141, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}

	reg = 0x23;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB142, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}

	reg = ctx->auto_speed_level;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 1, 0xB144, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}

	reg = 0x80;
	rc = spmi_ext_register_writel(ctx->spmi->ctrl, 0, 0xC346, &reg, 1);
	if (rc) {
		dev_err(ctx->dev, "Failed to write to SPMI, reg 0x%x: %zd", reg,
			rc);
		return len;
	}

	return len;
}

static DEVICE_ATTR(speed, 0664, fan_adjust_speed_show, fan_adjust_speed);
static DEVICE_ATTR(auto_speed, 0664, fan_adjust_auto_speed_show,
		   fan_adjust_auto_speed);

static struct attribute *coral_fan_attributes[] = {
	&dev_attr_speed.attr, &dev_attr_auto_speed.attr, NULL,
};

static struct attribute_group coral_fan_attr_group = {
	.attrs = coral_fan_attributes,
};

static int coral_fan_probe(struct spmi_device *spmi)
{
	int rc = 0;
	u8 reg;
	struct device_node *node;
	struct coral_fan_ctx *ctx;

	ctx = devm_kzalloc(&spmi->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		return -ENOMEM;
	}
	ctx->dev = &spmi->dev;
	ctx->spmi = spmi;
	ctx->speed_level = FAN_OFF_SPEED_LEVEL;

	node = spmi->dev.of_node;
	ctx->fan_en_gpio =
	    of_get_named_gpio(node, "oculus,coral-fan-en-gpio", 0);
	if (!gpio_is_valid(ctx->fan_en_gpio)) {
		rc = -EINVAL;
		dev_err(&spmi->dev, "Unable to read coral-fan-en-gpio");
		goto fail_alloc;
	}
	rc = gpio_request(ctx->fan_en_gpio, "fan_enable");
	if (rc) {
		dev_err(&spmi->dev, "Unable to request coral-fan-en-gpio: %d",
			rc);
		goto fail_alloc;
	}
	ctx->pwm = of_pwm_get(node, NULL);
	if (IS_ERR(ctx->pwm)) {
		rc = PTR_ERR(ctx->pwm);
		dev_err(&spmi->dev, "Unable to read PWM config: %d", rc);
		goto fail_gpio;
	}
	ctx->fan_led_cdev.name = "fan";
	rc = led_classdev_register(NULL, &ctx->fan_led_cdev);
	if (rc) {
		dev_err(&spmi->dev, "Unable to register fan device node: %d",
			rc);
		goto fail_pwm;
	}
	rc = sysfs_create_group(&ctx->fan_led_cdev.dev->kobj,
				&coral_fan_attr_group);
	if (rc) {
		dev_err(&spmi->dev, "Unable to create fan sysfs group: %d", rc);
		goto fail_led_classdev;
	}
	rc = gpio_direction_output(ctx->fan_en_gpio, 1);
	if (rc) {
		dev_err(&spmi->dev, "Unable to set initial fan GPIO state: %d",
			rc);
		goto fail_led_classdev;
	}
	rc = spmi_ext_register_readl(spmi->ctrl, 0, 0xC346, &reg, 1);
	if (rc) {
		dev_err(&spmi->dev, "Unable to read from SPMI, reg 0x%x: %d",
			reg, rc);
		goto fail_led_classdev;
	}
	if (!(reg & 0x80)) {
		reg = 0x80;
		rc = spmi_ext_register_writel(spmi->ctrl, 0, 0xC346, &reg, 1);
		if (rc) {
			dev_err(&spmi->dev,
				"Unable to write to SPMI, reg 0x%x: %d", reg,
				rc);
			goto fail_led_classdev;
		}
	}

	pwm_config(ctx->pwm, FAN_MIN_SPEED_DUTY_NS, FAN_DEFAULT_PERIOD_NS);
	pwm_enable(ctx->pwm);

	dev_set_drvdata(&spmi->dev, ctx);

	return rc;

fail_led_classdev:
	led_classdev_unregister(&ctx->fan_led_cdev);
fail_pwm:
	pwm_put(ctx->pwm);
fail_gpio:
	gpio_free(ctx->fan_en_gpio);
fail_alloc:
	devm_kfree(&spmi->dev, ctx);
	return rc;
}

static int coral_fan_remove(struct spmi_device *spmi)
{
	struct coral_fan_ctx *ctx = dev_get_drvdata(&spmi->dev);
	led_classdev_unregister(&ctx->fan_led_cdev);
	pwm_put(ctx->pwm);
	gpio_free(ctx->fan_en_gpio);
	devm_kfree(&spmi->dev, ctx);

	return 0;
}

static struct of_device_id coral_fan_match_table[] = {
	{ .compatible = "oculus,coral-fan" },
	{},
};

static struct spmi_driver coral_fan_driver = {
	.driver = {
		.name = "coral-fan",
		.owner = THIS_MODULE,
		.of_match_table = coral_fan_match_table,
	},
	.probe = coral_fan_probe,
	.remove = coral_fan_remove,
};

static int __init coral_init(void)
{
	return spmi_driver_register(&coral_fan_driver);
}

static void __exit coral_exit(void)
{
	spmi_driver_unregister(&coral_fan_driver);
}

module_init(coral_init);
module_exit(coral_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Platform driver for Coral board");
