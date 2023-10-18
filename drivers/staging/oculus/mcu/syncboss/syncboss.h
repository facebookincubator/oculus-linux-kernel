/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _SYNCBOSS_H
#define _SYNCBOSS_H

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/miscfifo.h>
#include <linux/of_platform.h>
#include <linux/spi/spi.h>
#include <linux/syncboss.h>
#include <linux/types.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <uapi/linux/sched/types.h>
#endif

#include "syncboss_protocol.h"
#include "../swd/swd.h"

#define SYNCBOSS_SEQ_NUM_MIN 1
#define SYNCBOSS_SEQ_NUM_MAX 254
#define SYNCBOSS_SEQ_NUM_BITS (SYNCBOSS_SEQ_NUM_MAX + 1)

/* Device stats */
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
 * A struct of fn pointers for SPI operations synboss will opt to call
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
	 * waiting for a full transactions's worth of data.
	 */
	u32 max_msg_send_delay_ns;

	/* Length of the fixed-size SPI transaction */
	u16 transaction_length;

	/* The rate to run the spi clock at (in Hz) */
	u32 spi_max_clk_rate;
};

/* Data related to clients */
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

struct syncboss_timing {
	u64 prev_trans_start_time_ns;
	u64 prev_trans_end_time_ns;
	u64 min_time_between_trans_ns;
	u64 trans_period_ns; /* used only for legacy polling mode */
};

/* Device state */
struct syncboss_dev_data {
	/*
	 * NOTE: swd_ops MUST be the first member of this structure.
	 * It is retrieved by child devices that expect it to be at
	 * the start of this (parent) device's drvdata.
	 */
	struct swd_ops swd_ops;

	/* Order does not matter for the internal properties below... */

	/* Function pointers for preparing and unpreparing SPI messages */
	struct spi_prepare_ops spi_prepare_ops;

	/* The parent SPI device */
	struct spi_device *spi;

	/* This driver uses the kernel's "misc driver" feature.  This
	 * device is used for sending data to the SyncBoss
	 */
	struct miscdevice misc;

	/* Data related to clients */
	u64 client_data_index;
	struct list_head client_data_list;

	/* In the new stream interface, we have a separate misc device
	 * just for reading stream data from the SyncBoss
	 */
	struct miscdevice misc_stream;
	/* FIFO to help push stream data from the SyncBoss to the
	 * misc_stream device.
	 */
	struct miscfifo stream_fifo;

	/* In the new stream interface, we have a separate misc device
	 * just for reading control data from the SyncBoss
	 */
	struct miscdevice misc_control;
	/* FIFO to help push control data from the SyncBoss to the
	 * misc_stream device.
	 */
	struct miscfifo control_fifo;

	/* Device to convey power state events and reasons
	 */
	struct miscdevice misc_powerstate;
	/* FIFO to help push state event data from the SyncBoss to the
	 * misc_powerstate device.
	 */
	struct miscfifo powerstate_fifo;

	/* Misc device for NSYNC
	 */
	struct miscdevice misc_nsync;
	struct miscfifo nsync_fifo;

	/* MCU power reference count */
	int mcu_client_count;

	/* Streaming client reference count */
	int streaming_client_count;

	/* nsync event client reference count */
	int nsync_client_count;

	/* Powertate event client reference count */
	int powerstate_client_count;

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
	 * The total number of SPI transactions that have ocurred
	 * since the MCU booted.
	 */
	atomic64_t transaction_ctr;

	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* GPIO line for pin reset */
	int gpio_reset;
	/* GPIO line for time sync */
	int gpio_timesync;
	/* Time sync IRQ */
	int timesync_irq;
	/* Data ready / wakeup line  */
	int gpio_ready;
	/* IRQ signaling the MCU is ready for a transaction */
	int ready_irq;

	/* GPIO NSYNC */
	int gpio_nsync;
	/* NSYNC IRQ */
	int nsync_irq;

	/* Timestamp of the last NSYNC event
	 * This signal is driven by the NRF using an otherwise unused
	 * NRF->AP signal and used to synchronize time between the AP
	 * and NRF.
	 */
	atomic_long_t nsync_irq_timestamp;
	u32 nsync_irq_count;

	/* Timestamp of last TE event
	 * This signal is driven by the display hardware's TE line to
	 * the both NRF and AP. This is used to synchronize time between
	 * the AP and NRF.
	 */
	atomic64_t last_te_timestamp_ns;

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

	/* Real-Time priority to use for spi polling thread */
	int poll_prio;

	/* Sequence number interface selection: ioctl if true, sysfs otherwise */
	bool has_seq_num_ioctl;

	/* The next sequence number available for a control call (sysfs) */
	int next_avail_seq_num;

	/* The last sequence number used for a control call (ioctl) */
	int last_seq_num;

	/* Bitmap of allocated sequence numbers (ioctl) */
	DECLARE_BITMAP(allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);

	u64 seq_num_allocation_count;

	/* True if the syncboss controlls a prox sensor */
	bool has_prox;

	/* True if prox calibration data is not required for prox to work */
	bool has_no_prox_cal;

	/* True if the MCU can be woken from shutdown via a SPI transaction (CS toggle) */
	bool has_wake_on_spi;

	/* prox calibration values */
	int prox_canc;
	int prox_thdl;
	int prox_thdh;

	/* prox config version */
	u8 prox_config_version;

	/* The most recent power state event */
	int powerstate_last_evt;

	/* True if the cameras are in-use */
	bool cameras_enabled;

	/* True if we should refrain from sending the next system_up event. */
	bool eat_next_system_up_event;

	/* True if we should refrain from sending the prox_on events. */
	bool eat_prox_on_events;

	/* True if we should not send any power state events to the user */
	bool silence_all_powerstate_events;

	/* True if power state events are enabled */
	bool powerstate_events_enabled;

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

	/* > 0 if data ready IRQ has fired but had not yet been handled */
	atomic64_t data_ready_fired_count;

	/* > 0 if data ready IRQ has fired but wake reasons have not yet been queried */
	u64 data_ready_fired_accum;

	/* True if a wakeup timer has fired but had not yet been handled */
	bool wake_timer_fired;

	/*
	 * True if MCU support IRQ-based transaction. False for
	 * legacy polling mode.
	 */
	bool use_irq_mode;

	struct dentry *dentry;
	struct dentry *clients_dentry;
};

int syncboss_init_sysfs_attrs(struct syncboss_dev_data *devdata);
void syncboss_deinit_sysfs_attrs(struct syncboss_dev_data *devdata);

void syncboss_pin_reset(struct syncboss_dev_data *devdata);

#endif
