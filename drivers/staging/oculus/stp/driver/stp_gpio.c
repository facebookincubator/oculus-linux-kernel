// SPDX-License-Identifier: GPL-2.0
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>

#include <common/stp_device_logging.h>
#include <driver/stp_gpio.h>

static int stp_gpio_parse_dt(struct device_node *const np,
		  struct stp_gpio_data *const data)
{
	if (!data || !np) {
		STP_DRV_LOG_ERR("bad gpio input");
		return -EINVAL;
	}

	data->device_request_transaction = of_get_named_gpio(np, "mcu-request-transaction", 0);
	if (data->device_request_transaction < 0) {
		STP_DRV_LOG_ERR("failed to get gpio mcu-request-transaction");
		return -EINVAL;
	}

	data->controller_has_data = of_get_named_gpio(np, "soc-has-data", 0);
	if (data->controller_has_data < 0) {
		STP_DRV_LOG_ERR("failed to get gpio soc-has-data");
		return -EINVAL;
	}

	return 0;
}

int stp_gpio_set_direction(struct stp_gpio_data *const data)
{
	if (!data) {
		STP_DRV_LOG_ERR("bad gpio input");
		return -EINVAL;
	}

	if (gpio_direction_input(data->device_request_transaction)) {
		STP_DRV_LOG_ERR("gpio wrong setting `%d`",
				data->device_request_transaction);
		return -EINVAL;
	}

	if (gpio_direction_output(data->controller_has_data, 1)) {
		STP_DRV_LOG_ERR("gpio wrong setting `%d`",
				data->controller_has_data);
		return -EINVAL;
	}

	return 0;
}

int stp_init_gpio(struct device_node *const np,
		  struct stp_gpio_data *const data)
{
	if (!data || !np) {
		STP_DRV_LOG_ERR("bad gpio input");
		return -EINVAL;
	}

	if (stp_gpio_parse_dt(np, data)) {
		STP_DRV_LOG_ERR("failed to parse gpio");
		return -EINVAL;
	}

	if (stp_gpio_set_direction(data)) {
		STP_DRV_LOG_ERR("failed to set gpio direction");
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

	*irq_number = gpio_to_irq(gpio);

	rval = devm_request_irq(dev, *irq_number, cb, irq_flags, label, NULL);
	if (rval) {
		STP_DRV_LOG_ERR("irq request failure %d", rval);
		goto exit_gpio;
	}

	return 0;

exit_gpio:
	return rval;
}

void stp_disable_gpio_irq(struct device *const dev,
			  struct stp_gpio_data *const data)
{
	disable_irq_wake(data->device_request_transaction_irq);
	disable_irq(data->device_request_transaction_irq);
}

int stp_config_gpio_irq(struct device *const dev,
			struct stp_gpio_data *const data,
			irqreturn_t (*device_request_transaction_cb)(int, void *))
{
	int rval;
	int irq_number;

	rval = stp_configure_single_gpio_irq(dev, data->device_request_transaction,
					     "MCU_request_transaction", device_request_transaction_cb,
					     IRQF_TRIGGER_FALLING, &irq_number);
	if (rval)
		return rval;

	data->device_request_transaction_irq = irq_number;

	// request transaction should be able to wake on suspend
	rval = enable_irq_wake(irq_number);
	if (rval) {
		STP_DRV_LOG_ERR("failed to set IRQ wake for `%d`", irq_number);
		stp_disable_gpio_irq(dev, data);
		return rval;
	}

	return 0;
}
