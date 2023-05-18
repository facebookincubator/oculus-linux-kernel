/* SPDX-License-Identifier: GPL-2.0 */
#ifndef STP_DRIVER_DATA_H
#define STP_DRIVER_DATA_H

#include <linux/completion.h>
#include <linux/atomic.h>
#include <linux/wait.h>

#include <driver/stp_gpio.h>

struct stp_driver_stats {
	atomic_t device_ready_irq_count;
	atomic_t data_irq_count;
};

struct spi_stp_driver_data {
	struct spi_device *spi;
	struct stp_gpio_data gpio_data;
	struct completion stp_thread_complete;
	struct task_struct *stp_thread;
	// Wait queue for the stp controller thread
	wait_queue_head_t thread_event_queue;
	struct stp_driver_stats stats;
	uint8_t *controller_tx_buffer;
	uint8_t *controller_rx_buffer;
	bool suspended;
	atomic_t has_data_in_suspend;
};

#endif
