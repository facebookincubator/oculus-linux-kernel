/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_SPI_H
#define _SYNCBOSS_SPI_H

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscfifo.h>
#include <linux/of_platform.h>
#include <linux/spi/spi.h>
#include <linux/syncboss/consumer.h>
#include <linux/syncboss/messages.h>
#include <linux/types.h>
#include <uapi/linux/sched/types.h>
#include <uapi/linux/syncboss.h>

#include "../swd/swd.h"

#define SYNCBOSS_DEVICE_NAME "syncboss0"

/* Sequence number settings */
#define SYNCBOSS_SEQ_NUM_MIN 1
#define SYNCBOSS_SEQ_NUM_MAX 254
#define SYNCBOSS_SEQ_NUM_BITS (SYNCBOSS_SEQ_NUM_MAX + 1)

/* SPI Thread Performance Tuning */
#define SYNCBOSS_DEFAULT_THREAD_PRIO 51
#define SYNCBOSS_SCHEDULING_SLOP_NS 10000

/*
 * Default for SPI transacton cadence and speed
 * Maybe changed at runtime via sysfs
 */
#define SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS 0
#define SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS 0
#define SYNCBOSS_DEFAULT_MAX_MSG_SEND_DELAY_NS 0
#define SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE 8000000

/* Max amount of time we give the MCU to wake or enter its sleep state */
#define SYNCBOSS_SLEEP_WAKE_TIMEOUT_MS 2000
#define SYNCBOSS_SLEEP_WAKE_TIMEOUT_WITH_GRACE_MS 5000

/* The amount of time we should hold the reset line low when doing a reset. */
#define SYNCBOSS_RESET_TIME_MS 5

/* SPI data magic numbers used to identify message validity and protocol versions */
#define SPI_TX_DATA_MAGIC_NUM 0xDEFEC8ED
#define SPI_RX_DATA_MAGIC_NUM_POLLING_MODE 0xDEFEC8ED
#define SPI_RX_DATA_MAGIC_NUM_IRQ_MODE 0xD00D1E8D
#define SPI_RX_DATA_MAGIC_NUM_MCU_BUSY 0xCACACACA

/*
 * The amount of time to hold the wake-lock when syncboss wakes up the
 * AP.  This allows user-space code to have time to react before the
 * system automatically goes back to sleep.
 */
#define SYNCBOSS_WAKEUP_EVENT_DURATION_MS 2000

/* The amount of time to suppress spurious SPI errors after a syncboss reset */
#define SYNCBOSS_RESET_SPI_SETTLING_TIME_MS 100

/* Maximum egress messages that may be enqueued */
#define MAX_MSG_QUEUE_ITEMS 100

/*
 * These are SPI message types that the driver explicitly monitors and
 * sends to MCU firmware. They must be kept in sync with the MCU values.
 */
#define SYNCBOSS_GET_DATA_MESSAGE_TYPE 2
#define SYNCBOSS_SHUTDOWN_MESSAGE_TYPE 90
#define SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE 244

/* Device stats (informational) */
struct syncboss_stats {
	u32 num_bad_magic_numbers;
	u32 num_bad_checksums;
	u32 num_invalid_transactions_received;
	u32 num_empty_transactions_received;
	u32 num_empty_transactions_sent;
	u32 num_transactions;

	s32 last_awake_dur_ms;
	s32 last_asleep_dur_ms;
};

/* A message to be sent/received */
struct syncboss_msg {
	struct list_head list;
	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;
	size_t transaction_bytes;
	struct syncboss_transaction tx;
};

/*
 * A struct of fn pointers for SPI operations syncboss will opt to call
 * manually, rather than having the kernel SPI framework call them as
 * part of spi_sync()
 */
struct spi_prepare_ops {
	int (*prepare_transfer_hardware)(struct spi_master *ctlr);
	int (*unprepare_transfer_hardware)(struct spi_master *ctlr);
	int (*prepare_message)(struct spi_master *ctlr,
			       struct spi_message *message);
	int (*unprepare_message)(struct spi_master *ctlr,
				 struct spi_message *message);
};

/* A set of SPI thread settings to apply */
struct syncboss_stream_settings {
	/*
	 * The desired interval of polled SPI transactions, when using
	 * legacy SPI polling mode.
	 */
	u32 trans_period_ns;

	/*
	 * The minimum time to wait between the end of a SPI
	 * transaction and the start of the next SPI transaction.
	 * Transactions triggered by data ready IRQs are not affected
	 * by this.
	 */
	u32 min_time_between_trans_ns;

	/*
	 * Maximum amount of time to delay sending of a message while
	 * waiting for a full transaction's worth of data.
	 */
	u32 max_msg_send_delay_ns;

	/* Length of the fixed-size SPI transaction */
	u16 transaction_length;

	/* The rate to run the spi clock at (in Hz) */
	u32 spi_max_clk_rate;
};

/* SPI thread timing parameters */
struct syncboss_timing {
	u64 prev_trans_start_time_ns;
	u64 prev_trans_end_time_ns;
	u64 min_time_between_trans_ns;
	u64 trans_period_ns; /* used only for legacy polling mode */
};

/* Data related to userspace clients of this driver */
struct syncboss_client_data {
	struct list_head list_entry;
	struct file *file;
	struct task_struct *task;
	u64 index;
	u64 seq_num_allocation_count;
	/* Bitmap of allocated sequence numbers (ioctl) */
	DECLARE_BITMAP(allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);
	struct dentry *dentry;
};

/* Per-transaction state */
struct transaction_context {
	bool msg_to_send;
	bool reschedule_needed;
	struct syncboss_msg *smsg;
	struct syncboss_msg *prepared_smsg;
	bool data_ready_irq_had_fired;
	bool send_timer_had_fired;
	bool wake_timer_had_fired;
};

/* Device state */
struct syncboss_dev_data {
	/*
	 * NOTE: consumer_ops MUST be the first member of this structure.
	 * It is retrieved by child devices that expect it to be at
	 * the start of this (parent) device's drvdata.
	 */
	struct syncboss_consumer_ops consumer_ops;

	/* Order does not matter for the internal properties below... */

	/* Function pointers for preparing and unpreparing SPI messages */
	struct spi_prepare_ops spi_prepare_ops;

	/* The parent SPI device */
	struct spi_device *spi;

	/* Primary misc device (ex. /dev/syncboss0) */
	struct miscdevice misc;

	/* Data related to clients */
	u64 client_data_index;
	struct list_head client_data_list;

	/* Notifier chain for MCU and streaming state changes. */
	struct raw_notifier_head state_event_chain;

	/* Notifier chain for subscribing to received packets. */
	struct raw_notifier_head rx_packet_event_chain;

	/* MCU power reference count */
	int mcu_client_count;

	/* Streaming client reference count */
	int streaming_client_count;

	/* Settings to use for next streaming session */
	struct syncboss_stream_settings next_stream_settings;

	/* Length of the SPI transactions. Not to be updated while streaming is active. */
	u16 transaction_length;

	/* The rate to run the spi clock at (in Hz). Not to be updated while streaming is active. */
	u32 spi_max_clk_rate;

	/* Time to wait before sending a less than full transaction. */
	u32 max_msg_send_delay_ns;

	/*
	 * For long work items we don't want to monopolize the system shared workqueue with.
	 * state_mutex must be held while enqueueing work due to syncboss_resume's locking
	 * pattern.
	 */
	struct workqueue_struct *syncboss_pm_workqueue;

	/* Used to wait for events on resume to complete before allowing suspend */
	struct completion pm_resume_completion;

	/*
	 * Handle to the task that is performing the SPI
	 * transactions
	 */
	struct task_struct *worker;

	/*
	 * Wakes the SPI handling thread to send messages rather than
	 * waiting for the nRF to signal data ready.
	 */
	struct hrtimer wake_timer;

	/* Wakes the SPI handling to send outgoing messages. */
	struct hrtimer send_timer;

	/*
	 * A pointer to a default message to send/receive if
	 * the send if msg_queue_list is empty.
	 */
	struct syncboss_msg *default_smsg;

	/* Queue of messages to send and related state. */
	struct mutex msg_queue_lock;
	int msg_queue_item_count;
	struct list_head msg_queue_list;

	/* Buffer for RX data */
	struct rx_history_elem *rx_elem;

	/*
	 * The total number of SPI transactions that have occurred
	 * since the MCU booted.
	 */
	atomic64_t transaction_ctr;

	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* GPIO line for pin reset */
	int gpio_reset;
	/* Data ready / wakeup line  */
	int gpio_ready;
	/* IRQ signaling the MCU is ready for a transaction */
	int ready_irq;

	/* Regulator for the nRF */
	struct regulator *mcu_core;

	/* Regulator for IMU */
	struct regulator *imu_core;

	/* Regulator for Magnetometer */
	struct regulator *mag_core;

	/* RF Power Amplifier */
	struct regulator *rf_amp;

	/* Hall Sensor Power  */
	struct regulator *hall_sensor;

	/* CPU cores used to schedule SPI transactions */
	struct cpumask cpu_affinity;

	/* Various statistics */
	struct syncboss_stats stats;

	/* Real-Time priority to use for spi thread */
	int thread_prio;

	/* The last sequence number used for a control call (ioctl) */
	int last_seq_num;

	/* Bitmap of allocated sequence numbers (ioctl) */
	DECLARE_BITMAP(allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);

	u64 seq_num_allocation_count;

	/* True if the MCU can be woken from shutdown via a SPI transaction (CS toggle) */
	bool has_wake_on_spi;

	/* True if streaming is running */
	bool is_streaming;

	/* True if a MCU wake-up has been handled since previous shutdown */
	bool wakeup_handled;

	/* The last time syncboss was reset (monotonic time in ms) */
	s64 last_reset_time_ms;

	/* True if we should force pin reset on next open. */
	bool force_reset_on_open;

	/* True if we should enable the fastpath spi code path */
	bool use_fastpath;

	/* True if data ready IRQ has fired but has not yet been handled */
	bool data_ready_fired;

	/* True if a wakeup timer has fired but has not yet been handled */
	bool wake_timer_fired;

	/* True if the send timer has fired but has not yet been handled */
	bool send_timer_fired;

	/*
	 * True if MCU support IRQ-based transaction. False for
	 * legacy polling mode.
	 */
	bool use_irq_mode;

	/* DebugFS nodes */
	struct dentry *dentry;
	struct dentry *clients_dentry;
};

int syncboss_init_sysfs_attrs(struct syncboss_dev_data *devdata);
void syncboss_deinit_sysfs_attrs(struct syncboss_dev_data *devdata);

void syncboss_pin_reset(struct syncboss_dev_data *devdata);

#endif
