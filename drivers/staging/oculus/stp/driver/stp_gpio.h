/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_GPIO_H
#define STP_GPIO_H

#include <linux/interrupt.h>

struct stp_gpio_data {
	unsigned int device_request_transaction;
	unsigned int device_request_transaction_irq;
	unsigned int controller_has_data;
};

int stp_gpio_set_direction(struct stp_gpio_data *const data);

int stp_init_gpio(struct device_node *const np,
		  struct stp_gpio_data *const data);

int stp_config_gpio_irq(struct device *const dev,
			struct stp_gpio_data *const data,
			irqreturn_t (*device_request_transaction_cb)(int, void *));

void stp_disable_gpio_irq(struct device *const dev,
			  struct stp_gpio_data *const data);
#endif
