// SPDX-License-Identifier: GPL-2.0
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>

#include <common/stp_device_logging.h>
#include <driver/stp_gpio.h>

int stp_init_gpio(struct device_node *const np,
		  struct stp_gpio_data *const data)
{
	if (!data || !np) {
		STP_DRV_LOG_ERR("bad gpio input");
		return -EINVAL;
	}

	data->device_has_data = of_get_named_gpio(np, "mcu-has-data", 0);
	if (gpio_direction_input(data->device_has_data)) {
		STP_DRV_LOG_ERR("gpio wrong setting `%d`",
				data->device_has_data);
		return -EINVAL;
	}

	data->device_can_receive = of_get_named_gpio(np, "mcu-ready", 0);
	if (gpio_direction_input(data->device_can_receive)) {
		STP_DRV_LOG_ERR("gpio wrong setting `%d`",
				data->device_can_receive);
		return -EINVAL;
	}

	return 0;
}

static int stp_configure_single_gpio_irq(struct device *const dev,
					 unsigned int gpio,
					 const char *const label,
					 irqreturn_t (*cb)(int, void *),
					 unsigned int irq_flags,
					 int *const irq_number)
{
	int rval;

	rval = devm_gpio_request(dev, gpio, label);
	if (rval) {
		STP_DRV_LOG_ERR("gpio request failure");
		return rval;
	}

	*irq_number = gpio_to_irq(gpio);

	rval = devm_request_irq(dev, *irq_number, cb, irq_flags, label, NULL);
	if (rval) {
		STP_DRV_LOG_ERR("irq request failure");
		goto exit_gpio;
	}

	return 0;

exit_gpio:
	return rval;
}

void stp_release_gpio_irq(struct device *const dev,
			  struct stp_gpio_data *const data)
{
	disable_irq(data->device_can_receive_irq);
	data->device_can_receive_irq = 0;

	disable_irq_wake(data->device_has_data_irq);
	disable_irq(data->device_has_data_irq);
	data->device_has_data_irq = 0;
}

int stp_config_gpio_irq(struct device *const dev,
			struct stp_gpio_data *const data,
			irqreturn_t (*has_data_cb)(int, void *),
			irqreturn_t (*device_ready_cb)(int, void *))
{
	int rval;
	int irq_number;

	rval = stp_configure_single_gpio_irq(dev, data->device_can_receive,
					     "MCU_ready", device_ready_cb,
					     IRQF_TRIGGER_RISING, &irq_number);
	if (rval)
		return rval;

	data->device_can_receive_irq = irq_number;

	rval = stp_configure_single_gpio_irq(dev, data->device_has_data,
					     "MCU_has_data", has_data_cb,
					     IRQF_TRIGGER_FALLING, &irq_number);
	if (rval)
		return rval;

	data->device_has_data_irq = irq_number;

	// Has data should be able to wake on suspend
	rval = enable_irq_wake(irq_number);
	if (rval) {
		STP_DRV_LOG_ERR("failed to set IRQ wake for `%d`", irq_number);
		stp_release_gpio_irq(dev, data);
		return rval;
	}

	return 0;
}
