// SPDX-License-Identifier: GPL-2.0
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mux/consumer.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

/*
 * There may be issues using this as a generic mux component as we do not
 * set the state of the pins to a safe state before switching and we cannot
 * guarantee the output state of unselected channels.
 *
 * When commanding a GPIO, we will make an implicit switch if needed. The
 * state of the GPIO behind the muxed output will be unchanged.
 *
 * ex. Setup 3 channel, 1 input mux (1 to 3)
 *      1. GPIO offset 2 on is currently selected as output/high.
 *         Channel 2 on mux is selected
 *      2. Next command is GPIO 0
 *         Channel 0 will be selected with GPIO output as output/high
 */

#define MCU_MUX_GET_CHANNEL(x, y) (x / y)
#define MCU_MUX_GET_IO_INDEX(x, y) (x % y)

struct mcu_mux {
	struct gpio_chip chip;

	struct mux_control *mux;
	int channel;
	int num_channels;

	int num_io;
	int *io;
};

static int mcu_mux_get_mux_control_if_needed(struct gpio_chip *chip)
{
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	if (PTR_ERR_OR_ZERO(mcumux->mux) == -EPROBE_DEFER)
		mcumux->mux = devm_mux_control_get(chip->parent, NULL);

	return PTR_ERR_OR_ZERO(mcumux->mux);
}

static int mcu_mux_set_channel_if_needed(struct gpio_chip *chip, int channel)
{
	int status;
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	if (channel == mcumux->channel)
		return 0;

	if (channel >= mcumux->num_channels)
		return -ENODEV;

	status = mcu_mux_get_mux_control_if_needed(chip);
	if (status) {
		dev_err(chip->parent,
			"Failed to get needed control %d with error: %d",
			channel, status);
		return status;
	}

	mux_control_deselect(mcumux->mux);
	status = mux_control_try_select(mcumux->mux, channel);
	if (status) {
		dev_err(chip->parent,
			"Failed to select proper channel: %d with error: %d",
			channel, status);
		return status;
	}

	mcumux->channel = channel;

	return 0;
}

static int mcu_mux_direction_input(struct gpio_chip *chip, unsigned int offset)
{
	int status;
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	status = mcu_mux_set_channel_if_needed(
		chip, MCU_MUX_GET_CHANNEL(offset, mcumux->num_channels));
	if (status)
		return status;

	return gpio_direction_input(
		mcumux->io[MCU_MUX_GET_IO_INDEX(offset, mcumux->num_io)]);
}

static int mcu_mux_direction_output(struct gpio_chip *chip, unsigned int offset,
				    int value)
{
	int status;
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	status = mcu_mux_set_channel_if_needed(
		chip, MCU_MUX_GET_CHANNEL(offset, mcumux->num_channels));
	if (status)
		return status;

	return gpio_direction_output(
		mcumux->io[MCU_MUX_GET_IO_INDEX(offset, mcumux->num_io)],
		value);
}

static int mcu_mux_get(struct gpio_chip *chip, unsigned int offset)
{
	int status;
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	status = mcu_mux_set_channel_if_needed(
		chip, MCU_MUX_GET_CHANNEL(offset, mcumux->num_channels));
	if (status)
		return status;

	return gpio_get_value(
		mcumux->io[MCU_MUX_GET_IO_INDEX(offset, mcumux->num_io)]);
}

static void mcu_mux_set(struct gpio_chip *chip, unsigned int offset, int value)
{
	int status;
	struct mcu_mux *mcumux = gpiochip_get_data(chip);

	status = mcu_mux_set_channel_if_needed(
		chip, MCU_MUX_GET_CHANNEL(offset, mcumux->num_channels));
	if (status)
		return;

	gpio_set_value(mcumux->io[MCU_MUX_GET_IO_INDEX(offset, mcumux->num_io)],
		       value);
}

static int mcu_mux_probe(struct platform_device *pdev)
{
	int index;
	int status;
	struct mcu_mux *mcumux;
	struct device_node *node = pdev->dev.of_node;

	mcumux = devm_kzalloc(&pdev->dev, sizeof(*mcumux), GFP_KERNEL);
	if (!mcumux)
		return -ENOMEM;

	status = of_property_read_u32(node, "oculus,num-channels",
				      &mcumux->num_channels);
	if (status) {
		dev_err(&pdev->dev, "Unable to get num-channels: %d", status);
		return status;
	}

	// Initialize mux channel to default
	status = of_property_read_u32(node, "oculus,default-channel",
				      &mcumux->channel);
	if (status) {
		dev_err(&pdev->dev,
			"Unable to get mux-control default channel: %d",
			status);
		return status;
	}

	mcumux->num_io = of_gpio_named_count(node, "oculus,muxed-pins");
	mcumux->io = devm_kcalloc(&pdev->dev, mcumux->num_io,
				  sizeof(*mcumux->io), GFP_KERNEL);
	if (!mcumux->io)
		return -ENOMEM;

	for (index = 0; index < mcumux->num_channels; index++) {
		mcumux->io[index] =
			of_get_named_gpio(node, "oculus,muxed-pins", index);
		if (!gpio_is_valid(mcumux->io[index]))
			return mcumux->io[index];
	}

	mcumux->mux = devm_mux_control_get(&pdev->dev, NULL);
	status = PTR_ERR_OR_ZERO(mcumux->mux);

	/*
	 * In order to support fast path GPIO control, we can ignore deferred
	 * probe errors so that we can immediately control the default mux
	 * output even if the mux itself is not available.
	 */
	if (status != -EPROBE_DEFER && status != 0) {
		dev_err(&pdev->dev, "Unable to get mux-control: %d", status);
		return status;
	}

	mcumux->chip.label = dev_name(&pdev->dev);
	mcumux->chip.ngpio = of_property_count_strings(node, "gpio-line-names");
	mcumux->chip.parent = &pdev->dev;
	mcumux->chip.of_node = pdev->dev.of_node;
	mcumux->chip.get = mcu_mux_get;
	mcumux->chip.set = mcu_mux_set;
	mcumux->chip.direction_input = mcu_mux_direction_input;
	mcumux->chip.direction_output = mcu_mux_direction_output;

	status = devm_gpiochip_add_data(&pdev->dev, &mcumux->chip, mcumux);
	if (status < 0) {
		dev_err(&pdev->dev, "Could not register gpiochip %d\n", status);
		return status;
	}

	return 0;
}

static const struct of_device_id mcu_mux_of_match[] = {
	{
		.compatible = "oculus,mcu-mux",
	},
	{ /* end of list */ }
};
MODULE_DEVICE_TABLE(of, mcu_mux_of_match);

static struct platform_driver mcu_mux_driver = {
	.probe = mcu_mux_probe,
	.driver = {
		.name = "oculus,mcu-mux",
		.of_match_table	= mcu_mux_of_match,
	},
};

static struct platform_driver *const drivers[] = {
	&mcu_mux_driver,
};

static int __init mcu_mux_late_init(void)
{
	return platform_register_drivers(drivers, ARRAY_SIZE(drivers));
}
late_initcall(mcu_mux_late_init);

static void __exit mcu_mux_exit(void)
{
	platform_unregister_drivers(drivers, ARRAY_SIZE(drivers));
}
module_exit(mcu_mux_exit);

MODULE_DESCRIPTION("MCU Mux GPIO Driver");
MODULE_LICENSE("GPL v2");
