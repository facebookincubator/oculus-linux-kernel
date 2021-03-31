#ifndef _SYNCBOSS_H
#define _SYNCBOSS_H

#include <linux/kernel.h>
#include <linux/miscfifo.h>
#include <linux/of_platform.h>
#include <linux/spi/spi.h>
#include <linux/syncboss.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <uapi/linux/sched/types.h>
#endif

#include "syncboss_protocol.h"
#include "swd.h"

/* Device stats */
struct syncboss_stats {
	u32 num_bad_magic_numbers;
	u32 num_bad_checksums;
	u32 num_rejected_transactions;
};

/* A message to be sent/received */
struct syncboss_msg {
	struct list_head list;
	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;
	bool finalized;
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

	/* Special device just for prox events
	 */
	struct miscdevice misc_prox;
	/* FIFO to help push prox data from the SyncBoss to the
	 * misc_prox device.
	 */
	struct miscfifo prox_fifo;

	/* Misc device for NSYNC
	 */
	struct miscdevice misc_nsync;
	struct miscfifo nsync_fifo;

	/* Connected clients reference count */
	int client_count;

	/* Connected prox clients reference count */
	int prox_client_count;

	/* The desired period of subsequent SPI transactions. */
	u32 transaction_period_ns;

	/* The minimum time to wait between the end of a SPI */
	/* transaction and the start of the next SPI transaction. */
	u32 min_time_between_transactions_ns;

	/* Length of the fixed-size SPI transaction */
	u16 transaction_length;

	/* The rate to run the spi clock at (in Hz) */
	u32 spi_max_clk_rate;

	/* Handle to the task that is performing the periodic SPI
	 * transactions
	 */
	struct task_struct *worker;

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

	/* The total number of SPI transactions that have ocurred so far */
	u64 transaction_ctr;

	/* Mutex that protects the state of this structure */
	struct mutex state_mutex;

	/* The workqueue serves 2 purposes:
	 *   1) It allows us to kick off work (such as streaming
	 *      start/stop) from execution contexts that couldn't do
	 *      such things directly (such as during driver probe).
	 *   2) Since this is a "singlethread" workqueue, it provides
	 *      a mechanism for us to serialize operations without the
	 *      need for crazy locking mechanisms.
	 */
	struct workqueue_struct *syncboss_workqueue;

	/* GPIO line for pin reset */
	int gpio_reset;
	/* GPIO line for time sync */
	int gpio_timesync;
	/* Time sync IRQ */
	int timesync_irq;
	/* AP Wakeup line  */
	int gpio_wakeup;
	/* Wakeup IRQ */
	int wakeup_irq;

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
	u64 nsync_irq_count;

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

	/* The next sequence number available for a control call */
	int next_avail_seq_num;

	/* Current power state of the device */
	int power_state;

	/* Indicates if driver initiated reset */
	bool reset_requested;

	/* Should we send headers with the data packets */
	bool enable_headers;

	/* True if the syncboss controlls a prox sensor */
	bool has_prox;

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	/* True if we must enable the camera temperature sensor
	 * regulator (needed for syncboss to function properly on
	 * pre-EVT3 units
	 */
	bool must_enable_camera_temp_sensor_power;
#endif

	/* prox calibration values */
	int prox_canc;
	int prox_thdl;
	int prox_thdh;

	/* prox config version */
	u8 prox_config_version;

	/* The most recent prox event */
	int prox_last_evt;

	/* True if the cameras are in-use */
	bool cameras_enabled;

	/* True if we should refrain from sending the next system_up event. */
	bool eat_next_system_up_event;

	/* True if we should not send any prox events to the user */
	bool silence_all_prox_events;

	/* True if prox wake is enabled */
	bool prox_wake_enabled;

	/* True if streaming is running */
	bool is_streaming;

	/* The last time syncboss was reset (monotonic time in ms) */
	s64 last_reset_time_ms;

	/* True if we should force pin reset on next open. */
	bool force_reset_on_open;

	/* Wakelock to prevent suspend while syncboss in use */
	struct wakeup_source syncboss_in_use_wake_lock;

	/* True if we should enable the fastpath spi code path */
	bool use_fastpath;
};

int syncboss_init_sysfs_attrs(struct syncboss_dev_data *devdata);
void syncboss_deinit_sysfs_attrs(struct syncboss_dev_data *devdata);

void syncboss_reset(struct syncboss_dev_data *devdata);

#endif
