
#ifndef STP_GPIO_H
#define STP_GPIO_H

#include <linux/interrupt.h>

struct stp_gpio_data {
	unsigned int device_has_data;
	unsigned int device_has_data_irq;
	unsigned int device_can_receive;
	unsigned int device_can_receive_irq;
};

int stp_init_gpio(struct device_node *const np,
		  struct stp_gpio_data *const data);

int stp_config_gpio_irq(struct device *const dev,
			struct stp_gpio_data *const data,
			irqreturn_t (*has_data_cb)(int, void *),
			irqreturn_t (*device_ready_cb)(int, void *));

void stp_release_gpio_irq(struct device *const dev,
			  struct stp_gpio_data *const data);

#endif
