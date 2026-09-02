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
	atomic_t first_transact_after_resume;
	// Count number of failed retries to enter suspend
	// because controller still has pending TX data available.
	atomic_t txdata_stuck_counter;
	// Count how many times the STP watchdog barked.
	atomic_t wdt_bark_counter;

	/** Cache of this GPIO state. */
	bool device_request_transaction_cache;

	/** Time stamp of the last time the device requested a transaction. */
	uint64_t device_request_transaction_timestamp;

	/* Raw arch-timer ticks, converted to ns only when dumped, to keep the
	 * division off the transaction and hard-IRQ paths. Single writer per
	 * field, so no lock: a dump is a best-effort sample (T272485417).
	 */
	uint64_t last_mcu_req_irq_tick;
	uint64_t last_soc_has_data_clear_tick;
	uint64_t last_suspend_tick;
	uint64_t last_resume_tick;

	/* Thread lifecycle: tells a parked STP thread from a merely idle one. */
	bool thread_alive;
	bool thread_parked;
	uint64_t last_thread_alive_tick;
	uint64_t last_thread_exit_tick;
	uint64_t last_thread_park_tick;
	uint64_t last_thread_unpark_tick;

	struct kernfs_node *txdata_stuck_counter_attr_node;
	struct kernfs_node *wdt_bark_counter_attr_node;

	bool enable_force_suspend;

	// Wakeup channel stats, +1 for invalid case
	uint32_t wakeups[STP_TOTAL_NUM_CHANNELS + 1];
};

#endif
