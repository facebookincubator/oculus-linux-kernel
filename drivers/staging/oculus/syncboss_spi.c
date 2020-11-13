#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/time64.h>
#include <linux/wait.h>
#include <linux/regulator/consumer.h>
#include <linux/miscfifo.h>
#include <linux/syncboss.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
#include <uapi/linux/sched/types.h>
#endif
#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
#include <syncboss_camera.h>
#endif

#include "fw_helpers.h"
#include "swd.h"
#include "spi_fastpath.h"

#ifdef CONFIG_OF /*Open firmware must be defined for dts useage*/
static const struct of_device_id oculus_syncboss_table[] = {
	{ .compatible = "oculus,syncboss" },
	{ },
};
static const struct of_device_id oculus_swd_match_table[] = {
	{ .compatible = "oculus,swd" },
	{ },
};
#else
	#define oculus_syncboss_table NULL
	#define oculus_swd_match_table NULL
#endif

/*
 * Logging Notes
 *  ================
 * In order to turn on verbose logging of every non-empty packet that
 * goes back-and-forth, you must use the dyndbg mechanism to enable
 * these logs.
 *
 * The simplest way to enable all of the syncboss verbose debug prints
 * is to run:
 *   echo module syncboss +p > /sys/kernel/debug/dynamic_debug/control
 *
 * For more info, see:
 *   http://lxr.free-electrons.com/source/Documentation/dynamic-debug-howto.txt
 */

#define SYNCBOSS_INTERFACE_PIPE_SIZE 4096

/* TODO: Support multiple instances? */
#define SYNCBOSS_DEVICE_NAME "syncboss0"
#define SYNCBOSS_STREAM_DEVICE_NAME "syncboss_stream0"
#define SYNCBOSS_CONTROL_DEVICE_NAME "syncboss_control0"
#define SYNCBOSS_PROX_DEVICE_NAME "syncboss_prox0"
#define SYNCBOSS_NSYNC_DEVICE_NAME "syncboss_nsync0"

#define SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS 0
#define SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS 0
#define SYNCBOSS_DEFAULT_TRANSACTION_LENGTH 128
#define SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE 8000000
#define SYNCBOSS_SCHEDULING_SLOP_US 5
/* The max amount of time we give SyncBoss to go into a sleep state after
 * issuing it a command to do so.
 */
#define SYNCBOSS_SLEEP_TIMEOUT_MS 2000

/* The amount of time to wait for space in the staging buffer to
 * become available when sending a request to syncboss
 */
#define SYNCBOSS_WRITE_STAGE_TIMEOUT_MS 100

/* Must be a power of 2 (since this maps to a kfifo size) */
#define SYNCBOSS_RX_HISTORY_SIZE 32

/* The amount of time we should hold the reset line low when doing a reset. */
#define SYNCBOSS_RESET_TIME_MS 5

#define SPI_DATA_MAGIC_NUM 0xDEFEC8ED

#define SPI_DATA_REJECTED_MAGIC_NUM 0xCACACACA

#define SYNCBOSS_MISCFIFO_SIZE 1024

#define SYNCBOSS_NSYNC_MISCFIFO_SIZE 256

#define SYNCBOSS_DEFAULT_POLL_PRIO 52

/* Valid sequence numbers are from [1, 254] */
#define SYNCBOSS_MIN_SEQ_NUM 1
#define SYNCBOSS_MAX_SEQ_NUM 254

/*
 * These are SPI message types that the driver explicitly monitors and
 * sends to SyncBoss firmware. They must be kept in sync.
 */
#define SYNCBOSS_SET_DATA_MESSAGE_TYPE 3
#define SYNCBOSS_CAMERA_PROBE_MESSAGE_TYPE 40
#define SYNCBOSS_CAMERA_RELEASE_MESSAGE_TYPE 41
#define SYNCBOSS_SHUTDOWN_MESSAGE_TYPE 90
#define SYNCBOSS_POWERSTATE_MESSAGE_TYPE 148
#define SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE 203
#define SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE 204
#define SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE 205
#define SYNCBOSS_PROXSTATE_MESSAGE_TYPE 207
#define SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE 212

#define INVALID_PROX_CAL_VALUE -1

#define DEFAULT_PROX_CONFIG_VERSION_VALUE 1

/* The amount of time to hold the wake-lock when syncboss wakes up the
 * AP.  This allows user-space code to have time to react before the
 * system automatically goes back to sleep.
 */
#define SYNCBOSS_WAKEUP_EVENT_DURATION_MS 2000

/* The amount of time to suppress spurious SPI errors after a syncboss
 * reset
 */
#define SYNCBOSS_RESET_SPI_SETTLING_TIME_MS 100

/* This struct is the overall structure of the SPI transaction data */
/* (in both directions). */
struct spi_transaction {
	/* Should always be SPI_DATA_MAGIC_NUM */
	uint32_t magic_num;
	/* Checksum for basic data integrity checks */
	uint8_t checksum;
	uint8_t data[];
} __packed;

/* The bulk of spi_transaction's are made up of one or more spi_data
 * packets.  The spi_data structure is just a generic container for
 * various types of data (IMU, controller, control, etc.)
 */
struct spi_data {
	/* The data type.  This is used to interpret the data */
	uint8_t type;
	/* The sequence id of the request/response (or 0 if N/A) */
	uint8_t sequence_id;
	/* The length of the data buffer */
	uint8_t data_len;
	uint8_t data[];
} __packed;

/* The message we send to SyncBoss to set the prox calibration */
struct prox_config_data {
	uint8_t msg_type;
	u16 prox_thdh;
	u16 prox_thdl;
	u16 prox_canc;
} __packed;

/* The message we send to SyncBoss to set the prox calibration */
struct prox_config_version {
	uint8_t msg_type;
	uint8_t config_version;
} __packed;

/* Describes the buffer that is currently being appended to (to be sent next) */
struct queued_buf_state {
	/* Pointer to start of the TX buffer */
	u8 *tx_buf_ptr;
	/* Pointer to start of the RX buffer */
	u8 *rx_buf_ptr;
	/* Pointer to next location to insert data to send */
	u8 *tx_insert_ptr;
};

/* Device stats */
struct syncboss_stats {
	u32 num_bad_magic_numbers;
	u32 num_bad_checksums;
	u32 num_rejected_transactions;
};

/* Power states the SyncBoss MCU can be in.  Note that these were
 * taken from the firmware's syncboss_registers.h and must match those
 * values.
 */
#define SYNCBOSS_POWER_STATE_RUNNING 0
#define SYNCBOSS_POWER_STATE_OFF 2

/* Device state */
struct syncboss_dev_data {
	/*
	 * NOTE: swd_ops MUST be the first member of this structure.
	 * It is retrieved by child devices that expect it to be at
	 * the start of this (parent) device's drvdata.
	 */
	struct swd_ops swd_ops;

	/* Order does not matter for the internal properties below... */

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

	/* TX is double-buffered */
	u8 *tx_bufs[2];
	/* Buffer for RX data */
	u8 *rx_buf;

	/* Keeps track of the state of the TX buffer that will be sent next */
	struct queued_buf_state queued_buf_state;
	/* Lock to protect the buffer that will be sent next */
	struct mutex queued_buf_mutex;

	/* The total number of SPI transactions that have ocurred so far */
	u64 transaction_ctr;

	/* Wait queue to signal clients that are waiting to write to
	 * the staging buffer that space may be available now
	 */
	wait_queue_head_t spi_tx_complete;

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
	uint8_t prox_config_version;

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

/* The version of the header the driver is currently using */
#define SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION SYNCBOSS_DRIVER_HEADER_VERSION_V1

/* This header is sent along with data from the syncboss mcu */
const struct syncboss_driver_data_header_t SYNCBOSS_MCU_DATA_HEADER = {
	.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
	.header_length = sizeof(struct syncboss_driver_data_header_t),
	.from_driver = false
};

/* This header is sent when the driver needs to signal a wakeup condition */
const struct syncboss_driver_data_header_driver_message_t
	SYNCBOSS_DRIVER_WAKEUP_HEADER = {
	{.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
	 .header_length =
		sizeof(struct syncboss_driver_data_header_driver_message_t),
	 .from_driver = true},
	.driver_message_type = SYNCBOSS_DRIVER_MESSAGE_WAKEUP
};

/* The header used to send prox state message to the prox fifo */
const struct syncboss_driver_data_header_driver_message_t
	SYNCBOSS_DRIVER_PROX_STATE_HEADER = {
	{.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
	 .header_length =
		sizeof(struct syncboss_driver_data_header_driver_message_t),
	 .from_driver = true},
	.driver_message_type = SYNCBOSS_DRIVER_MESSAGE_PROX_MSG
};

static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos);

/* Sysfs Attributes
 * ================
 * Note: If you add a new attribute, make sure you also set the
 *       correct ownership and permissions in the corresponding init
 *       script (such as
 *       device/oculus/monterey/rootdir/etc/init.monterey.rc)
 *
 * For most of these settings, the stream will have to be stopped/started again
 * for the settings to take effect.  So either set these before creating a
 * syncboss handle, or close/re-open the syncboss handle after changing these
 * settings.
 *
 * transaction_length - set the fixed size of the periodic SPI transaction
 * transaction_period_us - set the period of the SPI requests
 * minimum_time_between_transactions_us - set the minimum amount of time we
 *     should wait between SPI transactions
 * transaction_history_size - set the size of the transaction history
 * spi_max_clk_rate - set the maximum clock rate to use for SPI transactions
 *     (actual clock rate may be lower)

 *     have been rejected by SyncBoss (deprecated, use the stats file instead)
 * reset - set to 1 to reset the syncboss (toggle reset pin)
 * stats - show various driver stats
 * update_firmware - write 1 to do a firmware update (firmware must be under
 *      /vendor/firmware/syncboss.bin)
 * next_avail_seq_num - the next available sequence number for control calls
 * te_timestamp - timestamp of the last TE event
 */

static ssize_t store_reset(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count);

static ssize_t show_transaction_length(struct device *dev,
				       struct device_attribute *attr,
				       char *buf);
static ssize_t store_transaction_length(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

static ssize_t show_transaction_period_us(struct device *dev,
					  struct device_attribute *attr,
					  char *buf);
static ssize_t store_transaction_period_us(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count);

static ssize_t show_minimum_time_between_transactions_us(struct device *dev,
						struct device_attribute *attr,
						char *buf);
static ssize_t store_minimum_time_between_transactions_us(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count);

static ssize_t show_spi_max_clk_rate(struct device *dev,
				     struct device_attribute *attr, char *buf);
static ssize_t store_spi_max_clk_rate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count);

static ssize_t show_cpu_affinity(struct device *dev,
				 struct device_attribute *attr, char *buf);
static ssize_t store_cpu_affinity(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count);

static ssize_t show_stats(struct device *dev,
			  struct device_attribute *attr,
			  char *buf);

static ssize_t show_poll_priority(struct device *dev,
				  struct device_attribute *attr, char *buf);
static ssize_t store_poll_priority(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_next_avail_seq_num(struct device *dev,
				       struct device_attribute *attr, char *buf);

static ssize_t show_power(struct device *dev,
			  struct device_attribute *attr,
			  char *buf);

static ssize_t store_power(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count);

static ssize_t show_te_timestamp(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_num_cameras(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t show_enable_fastpath(struct device *dev,
				    struct device_attribute *attr,
				    char *buf);

static ssize_t store_enable_fastpath(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count);


static DEVICE_ATTR(reset, S_IWUSR, NULL, store_reset);
static DEVICE_ATTR(spi_max_clk_rate, S_IWUSR | S_IRUGO, show_spi_max_clk_rate,
		   store_spi_max_clk_rate);
static DEVICE_ATTR(transaction_period_us, S_IWUSR | S_IRUGO,
		   show_transaction_period_us, store_transaction_period_us);
static DEVICE_ATTR(minimum_time_between_transactions_us, S_IWUSR | S_IRUGO,
		   show_minimum_time_between_transactions_us,
		   store_minimum_time_between_transactions_us);
static DEVICE_ATTR(transaction_length, S_IWUSR | S_IRUGO,
		   show_transaction_length, store_transaction_length);
static DEVICE_ATTR(cpu_affinity, S_IWUSR | S_IRUGO,
		   show_cpu_affinity, store_cpu_affinity);
static DEVICE_ATTR(stats, S_IRUGO,
		   show_stats, NULL);
static DEVICE_ATTR(poll_prio, S_IWUSR | S_IRUGO,
		   show_poll_priority, store_poll_priority);
static DEVICE_ATTR(next_avail_seq_num, S_IRUGO,
		   show_next_avail_seq_num, NULL);
static DEVICE_ATTR(power, S_IWUSR | S_IRUGO, show_power, store_power);
static DEVICE_ATTR(te_timestamp, S_IRUGO, show_te_timestamp, NULL);
static DEVICE_ATTR(num_cameras, S_IRUGO, show_num_cameras, NULL);
static DEVICE_ATTR(enable_fastpath, S_IWUSR | S_IRUGO, show_enable_fastpath, store_enable_fastpath);

static struct attribute *syncboss_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_spi_max_clk_rate.attr,
	&dev_attr_transaction_period_us.attr,
	&dev_attr_minimum_time_between_transactions_us.attr,
	&dev_attr_transaction_length.attr,
	&dev_attr_cpu_affinity.attr,
	&dev_attr_stats.attr,
	&dev_attr_poll_prio.attr,
	&dev_attr_next_avail_seq_num.attr,
	&dev_attr_power.attr,
	&dev_attr_te_timestamp.attr,
	&dev_attr_num_cameras.attr,
	&dev_attr_enable_fastpath.attr,
	NULL
};

static struct attribute_group syncboss_attr_grp = {
	.name = "control",
	.attrs = syncboss_attrs
};

/* This is a function that miscfifo calls to determine if it should
 * send a given packet to a given client.
 */
static bool should_send_stream_packet(const void *context,
				      const u8 *header, size_t header_len,
				      const u8 *payload, size_t payload_len)
{
	int x = 0;
	const struct syncboss_driver_stream_type_filter *stream_type_filter =
		context;
	const struct spi_data *packet = (struct spi_data *)payload;

	/* Special case for when no filter is set or there's no payload */
	if (!stream_type_filter || (stream_type_filter->num_selected == 0) ||
	    !payload)
		return true;

	for (x = 0; x < stream_type_filter->num_selected; ++x) {
		if (packet->type == stream_type_filter->selected_types[x])
			return true;
	}
	return false;
}

static int read_cal_int(struct syncboss_dev_data *devdata,
			const char *cal_file_name)
{
	int status = 0;
	u32 temp_parse = 0;
	const struct firmware *fw = NULL;
	char tempstr[16] = {0};

	status = request_firmware(&fw, cal_file_name, &devdata->spi->dev);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"request_firmware() returned %d. Please ensure %s is present",
			status, cal_file_name);
		return status;
	}

	if (fw->size >= sizeof(tempstr)) {
		dev_err(&devdata->spi->dev,
			"Unexpected size for %s (size is %zd)",
			cal_file_name, fw->size);
		status = -EINVAL;
		goto error;
	}

	/* Copy to temp buffer to ensure null-termination */
	memcpy(tempstr, fw->data, fw->size);
	tempstr[fw->size] = '\0';

	status = kstrtou32(tempstr, /*base */10, &temp_parse);
	if (status < 0) {
		dev_err(&devdata->spi->dev, "Failed to parse integer out of %s",
			tempstr);
		goto error;
	}

	status = temp_parse;

error:
	release_firmware(fw);
	return status;
}

static int prox_cal_valid(struct syncboss_dev_data *devdata)
{
	/* Note: There are cases in the factory where they need to
	 * just set prox_canc without setting the other values.  Given
	 * this use-case, we consider a prox cal "valid" if anything
	 * is non-zero.
	 */
	return (devdata->prox_canc >= 0) || (devdata->prox_thdl >= 0)
		|| (devdata->prox_thdh >= 0);
}

static void read_prox_cal(struct syncboss_dev_data *devdata)
{
	devdata->prox_config_version = read_cal_int(devdata,
						    "PROX_PS_CAL_VERSION");
	devdata->prox_canc = read_cal_int(devdata, "PROX_PS_CANC");
	devdata->prox_thdl = read_cal_int(devdata, "PROX_PS_THDL");
	devdata->prox_thdh = read_cal_int(devdata, "PROX_PS_THDH");

	if (!prox_cal_valid(devdata)) {
		dev_err(&devdata->spi->dev,
			"Failed read prox calibration data (ver: %u, canc: %d, thdl: %d, thdh: %d)",
			devdata->prox_config_version, devdata->prox_canc,
			devdata->prox_thdl, devdata->prox_thdh);
		return;
	}

	dev_info(&devdata->spi->dev,
		 "Prox cal read: canc: %d, thdl: %d, thdh: %d",
		 devdata->prox_canc, devdata->prox_thdl, devdata->prox_thdh);
}


static int spi_queue_work(struct syncboss_dev_data *devdata,
			  fw_work_func_t func,
			  struct completion *work_complete)
{
	return fw_queue_work(devdata->syncboss_workqueue,
		devdata, func, work_complete);
}

static void start_streaming(void *devdata);
static void start_streaming_force_reset(void *devdata);
static void stop_streaming(void *devdata);
static int wait_for_syncboss_wake_state(struct syncboss_dev_data *devdata,
					bool state);

static void reset_syncboss(struct syncboss_dev_data *devdata)
{
	/* Start then stop the stream to pin-reset syncboss and tell it to
	 * sleep
	 */
	spi_queue_work(devdata, start_streaming_force_reset, NULL);
	wait_for_syncboss_wake_state(devdata, true);
	spi_queue_work(devdata, stop_streaming, NULL);
}

static bool is_busy(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	return devdata->is_streaming;
}

static void fw_update_cb(struct device *child, int status)
{
	struct syncboss_dev_data *devdata;

	if (status < 0)
		return;

	devdata = dev_get_drvdata(child->parent);
	reset_syncboss(devdata);
}

static int syncboss_inc_client_count(struct syncboss_dev_data *devdata)
{
	struct completion init_complete;

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->client_count < 0);

	/* The first time a client opens a handle to the driver, read
	 * the prox sensor calibration.  We do it here as opposed to
	 * in the workqueue thread because request_firmware must be
	 * called in a user-context.  We read the calibration every
	 * time to support the factory case where prox calibration is
	 * being modified and we don't want to force a headset reboot
	 * for the new calibration to take effect.
	 */
	if (devdata->has_prox)
		read_prox_cal(devdata);

	++devdata->client_count;

	if (devdata->client_count == 1) {
		dev_info(&devdata->spi->dev, "Starting streaming thread work");

		spi_queue_work(devdata, start_streaming, &init_complete);

		/* Wait for completion outside the lock */
		mutex_unlock(&devdata->state_mutex);
		wait_for_completion(&init_complete);
		/* Re-lock before returning */
		mutex_lock(&devdata->state_mutex);
	}
	return 0;
}

static int syncboss_dec_client_count(struct syncboss_dev_data *devdata)
{
	struct completion deinit_complete;

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->client_count < 1);

	--devdata->client_count;

	if (devdata->client_count == 0) {
		dev_info(&devdata->spi->dev, "Stopping streaming thread work");
		spi_queue_work(devdata, stop_streaming, &deinit_complete);

		/* Wait for completion outside the lock */
		mutex_unlock(&devdata->state_mutex);
		wait_for_completion(&deinit_complete);
		/* Re-lock before returning */
		mutex_lock(&devdata->state_mutex);
	}
	return 0;
}

static int fw_update_check_busy(struct device *dev, void *data)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd")) {
		struct swd_dev_data *devdata = dev_get_drvdata(dev);

		if (devdata->fw_update_state != FW_UPDATE_STATE_IDLE) {
			dev_err(dev, "Cannot open SyncBoss handle while firmware update is in progress");
			 return -EBUSY;
		}
	}
	return 0;
}

static int syncboss_open(struct inode *inode, struct file *f)
{
	int status;
	struct syncboss_dev_data *devdata =
	    container_of(f->private_data, struct syncboss_dev_data, misc);
	dev_info(&devdata->spi->dev, "SyncBoss handle opened (%s)",
		 current->comm);

	mutex_lock(&devdata->state_mutex);

	/* Ensure a FW update is not in progress */
	status = device_for_each_child(&devdata->spi->dev, NULL,
				       fw_update_check_busy);
	if (status != 0)
		goto out;

	syncboss_inc_client_count(devdata);

out:
	mutex_unlock(&devdata->state_mutex);

	return status;
}

static int signal_prox_event(struct syncboss_dev_data *devdata, int evt,
			     bool should_lock)
{
	int status = 0;
	bool should_update_last_evt = true;
	struct syncboss_driver_data_header_driver_message_t msg_to_send;

	if (should_lock) {
		status = mutex_lock_interruptible(&devdata->state_mutex);
		if (status != 0) {
			dev_err(&devdata->spi->dev,
				"Failed to get state mutex: %d",
				status);
			return status;
		}
	}

	if (!devdata->enable_headers) {
		dev_warn(&devdata->spi->dev,
			 "Prox events not supported without data headers");
		return 0;
	}

	memcpy(&msg_to_send, &SYNCBOSS_DRIVER_PROX_STATE_HEADER,
	       sizeof(msg_to_send));
	msg_to_send.driver_message_data = evt;

	if (evt == SYNCBOSS_PROX_EVENT_SYSTEM_UP)
		devdata->power_state = SYNCBOSS_POWER_STATE_RUNNING;
	else if (evt == SYNCBOSS_PROX_EVENT_SYSTEM_DOWN)
		devdata->power_state = SYNCBOSS_POWER_STATE_OFF;

	if ((evt == SYNCBOSS_PROX_EVENT_SYSTEM_UP) &&
	    devdata->eat_next_system_up_event) {
		/* This is a manual reset, so no need to notify clients */
		dev_info(&devdata->spi->dev,
			 "Eating prox system_up event on reset..yum!");
		devdata->eat_next_system_up_event = false;
		/* We don't want anyone who opens a prox handle to see this event. */
		should_update_last_evt = false;
	} else if (devdata->silence_all_prox_events) {
		dev_info(&devdata->spi->dev,
			 "Silencing prox event %d",
			 evt);
		/* We don't want anyone who opens a prox handle to see this event. */
		should_update_last_evt = false;
	} else {
		status = miscfifo_send_header_payload(
			&devdata->prox_fifo,
			(u8 *)&msg_to_send,
			sizeof(msg_to_send),
			(u8 *)&status,
			sizeof(status));
		if (status != 0)
			dev_warn_ratelimited(&devdata->spi->dev,
					     "Fifo overflow (%d)",
					     status);
	}

	if (should_update_last_evt) {
		devdata->prox_last_evt = evt;
	}

	if (should_lock)
		mutex_unlock(&devdata->state_mutex);
	return status;
}

static void prox_wake_enable(void *devdata);
static void prox_wake_disable(void *devdata);

static int syncboss_prox_open(struct inode *inode, struct file *f)
{
	struct completion prox_init_complete;
	int status = 0;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_prox);
	dev_info(&devdata->spi->dev, "SyncBoss prox handle opened (%s)",
		 current->comm);

	status = miscfifo_fop_open(f, &devdata->prox_fifo);
	if (status != 0)
		return status;

	mutex_lock(&devdata->state_mutex);
	++devdata->prox_client_count;

	/* Send the last prox_event */
	if (devdata->prox_last_evt != INVALID_PROX_CAL_VALUE) {
		dev_info(&devdata->spi->dev,
			 "Signaling prox_last_evt (%d)",
			 devdata->prox_last_evt);
		signal_prox_event(devdata, devdata->prox_last_evt,
				  /*should_lock*/false);
	}

	if (devdata->prox_client_count == 1) {
		if (devdata->has_prox)
			read_prox_cal(devdata);

		/* Enable prox wakeup */
		dev_info(&devdata->spi->dev, "Enabling prox wakeup");
		spi_queue_work(devdata, prox_wake_enable,
				    &prox_init_complete);

		/* Wait for completion outside the lock */
		mutex_unlock(&devdata->state_mutex);
		wait_for_completion(&prox_init_complete);
	} else
		mutex_unlock(&devdata->state_mutex);

	return status;
}

static int syncboss_prox_release(struct inode *inode, struct file *file)
{
	struct completion prox_deinit_complete;
	struct miscfifo_client *client = file->private_data;
	struct miscfifo *mf = client->mf;
	struct syncboss_dev_data *devdata =
		container_of(mf, struct syncboss_dev_data,
			     prox_fifo);

	mutex_lock(&devdata->state_mutex);
	--devdata->prox_client_count;

	if (devdata->prox_client_count == 0) {
		/* Disable prox wakeup */
		dev_info(&devdata->spi->dev, "Disabling prox wakeup");
		spi_queue_work(devdata, prox_wake_disable,
				    &prox_deinit_complete);

		/* Wait for completion outside the lock */
		mutex_unlock(&devdata->state_mutex);
		wait_for_completion(&prox_deinit_complete);
	} else
		mutex_unlock(&devdata->state_mutex);

	return miscfifo_fop_release(inode, file);
}

static int syncboss_stream_open(struct inode *inode, struct file *f)
{
	int status = 0;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_stream);
	dev_info(&devdata->spi->dev, "SyncBoss stream handle opened (%s)",
		 current->comm);

	status = miscfifo_fop_open(f, &devdata->stream_fifo);
	if (status != 0)
		return status;

	return status;
}

static int syncboss_set_stream_type_filter(struct file *file,
	const struct syncboss_driver_stream_type_filter __user *filter)
{
	int status = 0;
	struct syncboss_driver_stream_type_filter *new_filter = NULL;
	struct syncboss_driver_stream_type_filter *existing_filter =
		miscfifo_fop_get_context(file);

	if (existing_filter) {
		miscfifo_fop_set_context(file, NULL);
		kfree(existing_filter);
	}

	new_filter = kzalloc(sizeof(*new_filter), GFP_KERNEL);
	if (!new_filter)
		return -ENOMEM;

	status = copy_from_user(new_filter, filter, sizeof(*new_filter));
	if (status != 0) {
		pr_err("Failed to copy %d bytes from user stream filter\n",
		       status);
		return -EFAULT;
	}

	/* Sanity check new_filter */
	if (new_filter->num_selected > SYNCBOSS_MAX_FILTERED_TYPES) {
		pr_err("Sanity check of user stream filter failed (num_selected = %d)\n",
		       new_filter->num_selected);
		return -EINVAL;
	}

	miscfifo_fop_set_context(file, new_filter);
	return 0;
}

static long syncboss_stream_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	switch (cmd) {
	case SYNCBOSS_SET_STREAMFILTER_IOCTL:
		return syncboss_set_stream_type_filter(file,
			(struct syncboss_driver_stream_type_filter *)arg);
	default:
		pr_err("Unrecognized IOCTL %ul\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int syncboss_stream_release(struct inode *inode, struct file *file)
{
	int status = 0;
	struct syncboss_driver_stream_type_filter *stream_type_filter = NULL;

	stream_type_filter = miscfifo_fop_get_context(file);
	if (stream_type_filter) {
		miscfifo_fop_set_context(file, NULL);
		kfree(stream_type_filter);
	}

	status = miscfifo_fop_release(inode, file);
	return status;
}

static int syncboss_control_open(struct inode *inode, struct file *f)
{
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_control);
	dev_info(&devdata->spi->dev, "SyncBoss control handle opened (%s)",
		 current->comm);
	return miscfifo_fop_open(f, &devdata->control_fifo);
}

static s64 ktime_get_ms(void)
{
	return ktime_to_ms(ktime_get());
}

static void syncboss_reset(struct syncboss_dev_data *devdata)
{
	if (devdata->gpio_reset < 0) {
		dev_err(&devdata->spi->dev,
			"Cannot reset SyncBoss since reset pin was not specified in device tree");
		return;
	}

	dev_info(&devdata->spi->dev, "Pin-resetting SyncBoss");

	/* Since we're triggering a reset, no need to notify clients
	 * when syncboss comes back up.
	 */
	devdata->eat_next_system_up_event = true;

	devdata->last_reset_time_ms = ktime_get_ms();

	/* SyncBoss's reset pin is active-low */
	gpio_set_value(devdata->gpio_reset, 0);
	msleep(SYNCBOSS_RESET_TIME_MS);
	gpio_set_value(devdata->gpio_reset, 1);
}

static int syncboss_release(struct inode *inode, struct file *f)
{
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data, misc);

	mutex_lock(&devdata->state_mutex);
	syncboss_dec_client_count(devdata);
	mutex_unlock(&devdata->state_mutex);

	dev_info(&devdata->spi->dev, "SyncBoss handle closed");
	return 0;
}

static int syncboss_nsync_open(struct inode *inode, struct file *f)
{
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_nsync);
	dev_info(&devdata->spi->dev, "SyncBoss nsync handle opened (%s)",
		 current->comm);
	return miscfifo_fop_open(f, &devdata->nsync_fifo);
}

static const struct file_operations fops = {
	.open = syncboss_open,
	.release = syncboss_release,
	.read = NULL,
	.write = syncboss_write,
	.poll = NULL
};

static const struct file_operations stream_fops = {
	.open = syncboss_stream_open,
	.release = syncboss_stream_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll,
	.unlocked_ioctl = syncboss_stream_ioctl
};

static const struct file_operations control_fops = {
	.open = syncboss_control_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll
};

static const struct file_operations prox_fops = {
	.open = syncboss_prox_open,
	.release = syncboss_prox_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static const struct file_operations nsync_fops = {
	.open = syncboss_nsync_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static inline u8 calculate_checksum(const u8 *buf, u8 len)
{
	u8 x = 0, sum = 0;
	for (x = 0; x < len; ++x)
		sum += buf[x];
	return 0 - sum;
}

/* Initialize a new, empty, SPI transaction TX buffer */
static void init_queued_buf(struct syncboss_dev_data *devdata, int idx)
{
	struct spi_transaction *transaction = NULL;

	/* Simple double-buffer scheme, so the buffer idx better be 0 or 1 */
	BUG_ON((idx != 0) && (idx != 1));

	devdata->queued_buf_state.tx_buf_ptr = devdata->tx_bufs[idx];
	devdata->queued_buf_state.rx_buf_ptr =
		((struct rx_history_elem *)devdata->rx_buf)->buf;

	memset(devdata->queued_buf_state.tx_buf_ptr, 0,
	       devdata->transaction_length);
	memset(devdata->queued_buf_state.rx_buf_ptr, 0,
	       devdata->transaction_length);

	transaction =
		(struct spi_transaction *)devdata->queued_buf_state.tx_buf_ptr;
	transaction->magic_num = SPI_DATA_MAGIC_NUM;

	devdata->queued_buf_state.tx_insert_ptr = transaction->data;
}

/* Queue a packet to be sent out with the next SPI transaction */
static int queue_tx_packet(struct syncboss_dev_data *devdata, const void *data,
			   u8 len)
{
	int status = 0;
	struct spi_transaction *current_transaction = NULL;
	u8 bytes_left = 0;

	status = mutex_lock_interruptible(&devdata->queued_buf_mutex);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"Failed to get buffer sem with error %d", status);
		return status;
	}

	current_transaction =
		(struct spi_transaction *)devdata->queued_buf_state.tx_buf_ptr;
	bytes_left = devdata->transaction_length -
		(devdata->queued_buf_state.tx_insert_ptr -
		 devdata->queued_buf_state.tx_buf_ptr);
	BUG_ON(bytes_left >= devdata->transaction_length);

	if (len > bytes_left) {
		/* Not enough space in the buffer.  Don't log here
		 * since this is normal in some cases (and the caller
		 * may re-try later)
		 */
		status = -ENOBUFS;
		goto error;
	}

	/* Copy the data packet into our queued buffer and update our
	 * counters
	 */
	memcpy(devdata->queued_buf_state.tx_insert_ptr, data, len);
	devdata->queued_buf_state.tx_insert_ptr += len;

 error:
	mutex_unlock(&devdata->queued_buf_mutex);
	return status;
}

static int swap_and_stage_buffers(struct syncboss_dev_data *devdata,
				  int *idx_to_send)
{
	int status = 0;
	struct spi_transaction *transaction = NULL;
	u8 new_staging_buf_idx = 0xff;

	*idx_to_send = 0xff;

	status = mutex_lock_interruptible(&devdata->queued_buf_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev,
			"Failed to get buffer sem with error %d", status);
		return status;
	}
	transaction =
		(struct spi_transaction *)devdata->queued_buf_state.tx_buf_ptr;

	/* Swap the queued buffer with the one we previously sent */
	new_staging_buf_idx =
		(devdata->queued_buf_state.tx_buf_ptr == devdata->tx_bufs[0]) ?
		1 : 0;
	*idx_to_send = !new_staging_buf_idx;

	/* Re-init queued buf */
	init_queued_buf(devdata, new_staging_buf_idx);

	mutex_unlock(&devdata->queued_buf_mutex);

	/* No need to calculate the checksum under the lock */
	transaction->checksum = calculate_checksum((u8 *)transaction,
						   devdata->transaction_length);
	return 0;
}

static int distribute_packet(struct syncboss_dev_data *devdata,
			     struct spi_data *packet)
{
	/* Stream packets are known by their zero sequence id.  All
	 * other packets are control packets
	 */
	struct miscfifo *fifo_to_use = NULL;
	bool prox_state = false;
	int status = 0;

	if (packet->sequence_id == 0) {
		/* Prox state changes are treated specially and redirected to
		 * the prox device fifo
		 */
		if (packet->type == SYNCBOSS_PROXSTATE_MESSAGE_TYPE) {
			prox_state = packet->data[0];

			signal_prox_event(devdata,
					  prox_state ?
					  SYNCBOSS_PROX_EVENT_PROX_ON :
					  SYNCBOSS_PROX_EVENT_PROX_OFF,
					  /*should_lock*/ true);
			return 0;
		}

		fifo_to_use = &devdata->stream_fifo;
	} else {
		fifo_to_use = &devdata->control_fifo;
	}

	if (devdata->enable_headers) {
		status = miscfifo_send_header_payload(
			fifo_to_use, (u8 *)&SYNCBOSS_MCU_DATA_HEADER,
			sizeof(SYNCBOSS_MCU_DATA_HEADER), (u8 *)packet,
			sizeof(*packet) + packet->data_len);
	} else {
		status = miscfifo_send_header_payload(
			fifo_to_use, NULL, 0,
			(u8 *)packet, sizeof(*packet) + packet->data_len);
	}

	if (status != 0)
		dev_warn_ratelimited(&devdata->spi->dev, "Fifo overflow (%d)",
				     status);
	return status;
}

static int parse_and_distribute(struct syncboss_dev_data *devdata,
				void *rx_buf)
{
	/* Note: This function assumes that rx_buf contains valid data
	 * (magic num and checksum should have been validated prior to
	 * calling this function)
	 */
	int status = 0;
	struct rx_history_elem *this_history_elem =
		(struct rx_history_elem *)rx_buf;
	struct spi_transaction *trans =
		(struct spi_transaction *)this_history_elem->buf;
	struct spi_data *current_packet = (struct spi_data *)trans->data;
	const uint8_t *trans_end = (uint8_t *)trans +
		devdata->transaction_length;

	/* Iterate over the packets and distribute them to either the
	 * stream or control fifos
	 */
	while (current_packet->type != 0) {
		/* Sanity check that the packet is entirely contained
		 * within the rx buffer and doesn't overflow
		 */
		uint8_t *pkt_end = (uint8_t *)current_packet +
			sizeof(struct spi_data) + current_packet->data_len;

		if (pkt_end > trans_end) {
			dev_err_ratelimited(&devdata->spi->dev, "Data packet overflow");
			return -EINVAL;
		}

		distribute_packet(devdata, current_packet);
		/* Next packet */
		current_packet = (struct spi_data *)pkt_end;
	}
	return status;
}

static bool any_data_in_spi_packet(const void *spi_buf)
{
	/* return true if there's any data in the packet (other than the magic
	 * number and checksum)
	 */
	return ((struct spi_transaction *)spi_buf)->data[0] != 0;
}

static bool recent_reset_event(struct syncboss_dev_data *devdata)
{
	s64 current_time_ms = ktime_get_ms();

	if ((current_time_ms - devdata->last_reset_time_ms) <=
	    SYNCBOSS_RESET_SPI_SETTLING_TIME_MS)
		return true;
	return false;
}

static bool spi_nrf_sanity_check_packet(struct syncboss_dev_data *devdata,
					const void *spi_buf)
{
	struct spi_transaction *trans = (struct spi_transaction *)spi_buf;

	if (trans->magic_num != SPI_DATA_MAGIC_NUM) {
		/* To avoid noise in the log, only log an error if we
		 * haven't recently reset the mcu
		 */
		if (!recent_reset_event(devdata))
			dev_err_ratelimited(&devdata->spi->dev,
					    "Bad magic number detected: 0x%08x",
					    trans->magic_num);
		++devdata->stats.num_bad_magic_numbers;
		return false;
	}

	if (calculate_checksum((u8 *)trans, devdata->transaction_length) != 0) {
		dev_err_ratelimited(&devdata->spi->dev, "Bad checksum detected");
		++devdata->stats.num_bad_checksums;
		return false;
	}

	/* If we've made it this far, the magic number and checksum
	 * look good, so this is most likely valid data.
	 */
	return true;
}

static void process_rx_data(struct syncboss_dev_data *devdata, void *rx_buf,
			    unsigned int len,
			    u64 prev_transaction_start_time_ns,
			    u64 prev_transaction_end_time_ns)
{
	struct rx_history_elem *this_history_elem =
		(struct rx_history_elem *)
		devdata->rx_buf;

	/* See "logging notes" section at the top of
	 * this file
	 */
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("syncboss received: ",
			     DUMP_PREFIX_OFFSET,
			     rx_buf,
			     len);
#endif

	this_history_elem->rx_ctr = ++devdata->transaction_ctr;
	this_history_elem->transaction_start_time_ns =
		prev_transaction_start_time_ns;
	this_history_elem->transaction_end_time_ns =
		prev_transaction_end_time_ns;
	this_history_elem->transaction_length = devdata->transaction_length;

	parse_and_distribute(devdata, devdata->rx_buf);
}

static int syncboss_spi_transfer_thread(void *ptr)
{
	int status = 0;
	struct spi_device *spi = (struct spi_device *)ptr;
	static struct spi_message spi_msg;
	static struct spi_transfer spi_xfer;
	struct syncboss_dev_data *devdata = NULL;
	u64 prev_transaction_start_time_ns = 0;
	u64 prev_transaction_end_time_ns = 0;
	u32 spi_max_clk_rate = 0;
	u16 transaction_length = 0;
	int transaction_period_ns = 0;
	int min_time_between_transactions_ns = 0;
	bool should_retry_prev_transaction = false;
	bool headers_enabled = false;
	bool fastpath_enabled = false;

	devdata = (struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);

	/* Grab some config options while under the state lock */
	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"Failed to get state mutex with error %d", status);
		return status;
	}
	spi_max_clk_rate = devdata->spi_max_clk_rate;
	transaction_length = devdata->transaction_length;
	transaction_period_ns = devdata->transaction_period_ns;
	min_time_between_transactions_ns =
		devdata->min_time_between_transactions_ns;
	headers_enabled = devdata->enable_headers;
	fastpath_enabled = devdata->use_fastpath;

	mutex_unlock(&devdata->state_mutex);

	dev_info(&spi->dev, "Entering SPI transfer loop");
	dev_info(&spi->dev, "  Max Clk Rate            : %d Hz",
		 spi_max_clk_rate);
	dev_info(&spi->dev, "  Trans. Len              : %d",
		 transaction_length);
	dev_info(&spi->dev, "  Trans. Period           : %d us",
		 transaction_period_ns / 1000);
	dev_info(&spi->dev, "  Min time between trans. : %d us",
		 min_time_between_transactions_ns / 1000);
	dev_info(&spi->dev, "  Headers enabled         : %s",
		 headers_enabled ? "yes" : "no");
	dev_info(&spi->dev, "  Fastpath enabled        : %s",
		 fastpath_enabled ? "yes" : "no");

	if (fastpath_enabled) {
		spi_fastpath_init(spi);
	}
	
	while (!kthread_should_stop()) {
		int idx_to_send = 0;
		u64 time_to_wait_ns = min_time_between_transactions_ns;

		if (!should_retry_prev_transaction) {
			status = swap_and_stage_buffers(devdata, &idx_to_send);
			if (status != 0) {
				dev_err(&devdata->spi->dev,
					"swap_and_stage_buffers failed: %d",
					status);
				goto error;
			}

			/* If anyone is waiting to stage data to send,
			 * now is a good time to do so.
			 */
			wake_up_interruptible(&devdata->spi_tx_complete);

			spi_message_init(&spi_msg);
			memset(&spi_xfer, 0, sizeof(spi_xfer));

			spi_xfer.tx_buf = devdata->tx_bufs[idx_to_send];
			spi_xfer.rx_buf =
			       ((struct rx_history_elem *)devdata->rx_buf)->buf;
			spi_xfer.len = transaction_length;
			spi_xfer.bits_per_word = 8;
			spi_xfer.speed_hz = spi_max_clk_rate;

			spi_message_add_tail(&spi_xfer, &spi_msg);
		}

		/* Check if we have to wait a bit before initiating the next SPI
		 * transaction
		 */
		if (prev_transaction_start_time_ns != 0) {
			u64 time_since_prev_transaction_ns =
				prev_transaction_end_time_ns -
				prev_transaction_start_time_ns;
			if (time_since_prev_transaction_ns <
			    transaction_period_ns) {
				time_to_wait_ns = max(time_to_wait_ns,
					       transaction_period_ns -
					       time_since_prev_transaction_ns);
			}
		}

		{
			u64 time_to_wait_us = time_to_wait_ns / NSEC_PER_USEC;
			/* The usleep_range function uses hrtimers so it's very
			 * precise. Giving this scheduler some leeway in when to
			 * wake us up helps save power (timers can be
			 * coalesced).
			 */
			usleep_range((time_to_wait_us <
			      SYNCBOSS_SCHEDULING_SLOP_US) ? 0 :
				 time_to_wait_us - SYNCBOSS_SCHEDULING_SLOP_US,
				 time_to_wait_us + SYNCBOSS_SCHEDULING_SLOP_US);
		}

#if defined(CONFIG_DYNAMIC_DEBUG)
		if (any_data_in_spi_packet(spi_xfer.tx_buf)) {
			/* See "logging notes" section at the top of this
			 * file
			 */
			print_hex_dump_bytes("syncboss sending: ",
					    DUMP_PREFIX_OFFSET, spi_xfer.tx_buf,
					    spi_xfer.len);
		}
#endif

		prev_transaction_start_time_ns = ktime_get_ns();
		if (fastpath_enabled) {
			status = spi_fastpath_transfer(spi, &spi_msg);
		} else {
			status = spi_sync(spi, &spi_msg);
		}
		prev_transaction_end_time_ns = ktime_get_ns();

		if (status != 0) {
			dev_err(&spi->dev, "spi_sync failed with error %d",
				status);

			if (status == -ETIMEDOUT) {
				/* Retry if needed and keep going */
				goto retry;
			}

			break;
		} else if (spi_nrf_sanity_check_packet(devdata,
						       spi_xfer.rx_buf)) {
			if (any_data_in_spi_packet(spi_xfer.rx_buf)) {
				process_rx_data(devdata, spi_xfer.rx_buf,
						spi_xfer.len,
						prev_transaction_start_time_ns,
						prev_transaction_end_time_ns);
			}
			should_retry_prev_transaction = false;
		} else {
			/* To avoid noise in the log, only log an error if we
			 * haven't recently reset the mcu
			 */
			if (!recent_reset_event(devdata))
				dev_warn_ratelimited(&spi->dev,
					"SPI transaction rejected by SyncBoss");
			devdata->stats.num_rejected_transactions++;

retry:
			/* We only have to retry a transaction if we were
			 * actually sending some request/command to the
			 * SyncBoss.  Otherwise, we can just move on.
			 */
			if (any_data_in_spi_packet(spi_xfer.tx_buf))
				should_retry_prev_transaction = true;
		}
	}

 error:
	if (fastpath_enabled) {
		spi_fastpath_deinit(spi);
	}

	while (!kthread_should_stop()) {
		msleep_interruptible(1);
	}
	dev_info(&spi->dev, "Streaming thread stopped");
	return status;
}

static void push_prox_cal_and_enable_wake(struct syncboss_dev_data *devdata,
					  bool enable)
{
	u8 message_buf[sizeof(struct spi_data) +
		       sizeof(struct prox_config_data)] = {};
	struct spi_data *message = (struct spi_data *)message_buf;
	struct prox_config_version *prox_cfg_version =
		(struct prox_config_version *)message->data;
	struct prox_config_data *prox_cal =
		(struct prox_config_data *)message->data;
	u8 data_len = 0;

	if (!devdata->has_prox)
		return;
	else if (!prox_cal_valid(devdata)) {
		dev_warn(&devdata->spi->dev,
			 "Not pushing prox cal since it's invalid");
		return;
	}

	if (enable)
		dev_info(&devdata->spi->dev,
			 "Pushing prox cal to device and enabling prox wakeup");
	else
		dev_info(&devdata->spi->dev, "Disabling prox wakeup");

	if (enable) {
		/* Only set prox config / calibration if we're enabling prox */

		/* Set prox config version */
		message->type = SYNCBOSS_SET_DATA_MESSAGE_TYPE;
		message->sequence_id = 0;
		message->data_len = sizeof(*prox_cfg_version);

		prox_cfg_version->msg_type =
			SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE;
		prox_cfg_version->config_version = devdata->prox_config_version;

		data_len = sizeof(struct spi_data) + message->data_len;
		queue_tx_packet(devdata, message, data_len);

		/* Set prox calibration */
		message->type = SYNCBOSS_SET_DATA_MESSAGE_TYPE;
		message->sequence_id = 0;
		message->data_len = sizeof(*prox_cal);

		prox_cal->msg_type = SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE;
		prox_cal->prox_thdh = (u16)devdata->prox_thdh;
		prox_cal->prox_thdl = (u16)devdata->prox_thdl;
		prox_cal->prox_canc = (u16)devdata->prox_canc;

		data_len = sizeof(struct spi_data) + message->data_len;
		queue_tx_packet(devdata, message, data_len);

	}

	/* Enable or disable prox */
	message->type = enable ? SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE :
		SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE;
	message->sequence_id = 0;
	message->data_len = 0;

	data_len = sizeof(struct spi_data) + message->data_len;
	queue_tx_packet(devdata, message, data_len);
}

static bool is_mcu_awake(const struct syncboss_dev_data *devdata)
{
	if (gpio_is_valid(devdata->gpio_wakeup))
		return gpio_get_value(devdata->gpio_wakeup) == 1;
	return true;
}

static int wait_for_syncboss_wake_state(struct syncboss_dev_data *devdata,
					bool awake)
{
	int x;

	for (x = 10; x <= SYNCBOSS_SLEEP_TIMEOUT_MS; x += 10) {
		msleep(10);
		if (is_mcu_awake(devdata) == awake) {
			dev_info(&devdata->spi->dev,
				 "SyncBoss is %s (after %d ms)",
				 awake ? "awake" : "asleep", x);
			devdata->power_state =
				awake ? SYNCBOSS_POWER_STATE_RUNNING :
				SYNCBOSS_POWER_STATE_OFF;
			return 0;
		}
	}
	dev_err(&devdata->spi->dev,
		"SyncBoss failed to %s within %dms.",
		awake ? "wake" : "sleep", SYNCBOSS_SLEEP_TIMEOUT_MS);
	return -ETIMEDOUT;
}

static void start_streaming_impl(struct syncboss_dev_data *devdata,
				 bool force_reset)
{
	int status = 0;
	bool mcu_awake;
	/* Set streaming thread to high real-time priority since the data
	 * we're getting over SPI is very timing-sensitive.
	 */
	struct sched_param spi_thread_scheduler_settings = {
		.sched_priority = devdata->poll_prio };

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(&devdata->spi->dev, "Failed to get state lock with error %d",
			status);
		return;
	}

	/* Grab a wake lock so we'll reject device suspend requests
	 * while in active-use
	 */
	__pm_stay_awake(&devdata->syncboss_in_use_wake_lock);

	if (devdata->hall_sensor) {
		status = regulator_enable(devdata->hall_sensor);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"Failed to enable hall sensor power: %d",
					status);
			goto error;
		}
	}

	if (devdata->mcu_core) {
		status = regulator_enable(devdata->mcu_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"Failed to enable MCU power: %d",
					status);
			goto error;
		}
	}

	if (devdata->imu_core) {
		status = regulator_enable(devdata->imu_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"Failed to enable IMU power: %d",
					status);
			goto error;
		}
	}

	if (devdata->mag_core) {
		status = regulator_enable(devdata->mag_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"Failed to enable mag power: %d",
					status);
			goto error;
		}
	}

	if (devdata->rf_amp) {
		status = regulator_enable(devdata->rf_amp);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"Failed to enable rf amp power: %d",
					status);
			goto error;
		}
	}

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	if (devdata->must_enable_camera_temp_sensor_power) {
		int camera_temp_power_status = 0;

		dev_info(&devdata->spi->dev,
			 "Enabling camera temperature sensor power");
		camera_temp_power_status = enable_camera_temp_sensor_power();

		if (camera_temp_power_status == 0) {
			dev_info(&devdata->spi->dev,
				 "Successfully enabled temp sensor power");
			/* Only need to do this once */
			devdata->must_enable_camera_temp_sensor_power = false;
		} else if (camera_temp_power_status == -EAGAIN) {
			/* If this is really early in the boot
			 * process, this operation might fail.  This
			 * is ok, we'll just get it later.
			 */
			dev_info(&devdata->spi->dev,
				 "Camera temperature sensors not ready yet. Will try again later");
		} else {
			dev_warn(&devdata->spi->dev,
				 "Failed to enable temp sensor power: %d.",
				 camera_temp_power_status);
		}
	}
#endif

	/* As an optimization, only reset the syncboss mcu if it's not
	 * currently running (or force_reset is specified).  We should
	 * be in a clean state regardless.  Here are the 2 cases to
	 * consider:
	 *
	 *   1) The mcu is asleep and someone just called
	 *   syncboss_init.  In this case we need to pin-reset the mcu
	 *   to bring it up.
	 *
	 *   2) The mcu is awake.  This will be the case iff the mcu
	 *   was woken by an external trigger (ie. prox).  In this
	 *   case, the mcu is freshly booted so we're in a good/clean
	 *   state already.
	 */
	mcu_awake = is_mcu_awake(devdata);

	if (force_reset || !mcu_awake || devdata->force_reset_on_open) {
		dev_info(&devdata->spi->dev,
			 "Resetting mcu (force: %d, mcu awake: %d, force on open: %d)",
			 !!force_reset,
			 !!mcu_awake,
			 !!devdata->force_reset_on_open);
		syncboss_reset(devdata);
		devdata->force_reset_on_open = false;
	} else {
		dev_info(&devdata->spi->dev,
			 "Skipping mcu reset since it's already awake");
	}

	dev_info(&devdata->spi->dev, "Starting stream");
	devdata->transaction_ctr = 0;
	init_queued_buf(devdata, 0);

	/* The worker thread should not be running yet, so the worker
	 * should always be null here */
	BUG_ON(devdata->worker != NULL);

	devdata->worker = kthread_create(syncboss_spi_transfer_thread,
					 devdata->spi, "syncboss:spi_thread");
	if (IS_ERR(devdata->worker)) {
		dev_err(&devdata->spi->dev, "Failed to start kernel thread. (%ld)",
			PTR_ERR(devdata->worker));
		devdata->worker = NULL;
		goto error;
	}

	dev_info(&devdata->spi->dev, "Setting SPI thread cpu affinity: %*pb",
		cpumask_pr_args(&devdata->cpu_affinity));
	kthread_bind_mask(devdata->worker, &devdata->cpu_affinity);
	wake_up_process(devdata->worker);

	status = sched_setscheduler(devdata->worker, SCHED_FIFO,
				    &spi_thread_scheduler_settings);

	if (status) {
		dev_warn(&devdata->spi->dev, "Failed to set SCHED_FIFO. (%d)",
			 status);
	}

	push_prox_cal_and_enable_wake(devdata, devdata->prox_wake_enabled);

	devdata->is_streaming = true;
error:
	mutex_unlock(&devdata->state_mutex);
}

static void start_streaming_force_reset(void *devdata)
{
	start_streaming_impl((struct syncboss_dev_data *) devdata,
		/*force_reset*/true);
}

static void start_streaming(void *devdata)
{
	start_streaming_impl((struct syncboss_dev_data *) devdata,
		/*force_reset*/false);
}

static void shutdown_syncboss_mcu(struct syncboss_dev_data *devdata)
{
	int is_asleep = 0;
	struct spi_data message = {
		.type = SYNCBOSS_SHUTDOWN_MESSAGE_TYPE,
		.sequence_id = 0,
		.data_len = 0
	};

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));

	dev_info(&devdata->spi->dev, "Telling SyncBoss to go to sleep");

	/* Send command to put the MCU to sleep */
	queue_tx_packet(devdata, &message, sizeof(message));

	/* Wait for shutdown.  The gpio_wakeup line will go low when things are
	 * fully shut down
	 */
	is_asleep = (wait_for_syncboss_wake_state(devdata,
						  /*awake*/false) == 0);

	if (!is_asleep) {
		dev_err(&devdata->spi->dev,
			"SyncBoss failed to sleep within %dms. Forcing reset on next open.",
			SYNCBOSS_SLEEP_TIMEOUT_MS);
			devdata->force_reset_on_open = true;
	}
}

static void syncboss_on_camera_release(struct syncboss_dev_data *devdata)
{
	dev_info(&devdata->spi->dev, "Turning off cameras");

	devdata->cameras_enabled = false;
#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	disable_cameras();
#endif
}

static void stop_streaming_impl(struct syncboss_dev_data *devdata)
{
	int status;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"Failed to get state lock with error %d",
			status);
		return;
	}

	/* Tell syncboss to go to sleep */
	shutdown_syncboss_mcu(devdata);

	if (devdata->cameras_enabled) {
		dev_warn(&devdata->spi->dev,
			"Cameras still enabled at shutdown. Manually forcing camera release");
		syncboss_on_camera_release(devdata);
	}

	devdata->is_streaming = false;
	mutex_unlock(&devdata->state_mutex);

	dev_info(&devdata->spi->dev, "Stopping stream");
	if (devdata->worker) {
		kthread_stop(devdata->worker);
		devdata->worker = NULL;
		/* Now that device comm is totally down, signal the SYSTEM_DOWN
		 * event
		 */
		signal_prox_event(devdata,
				  SYNCBOSS_PROX_EVENT_SYSTEM_DOWN,
				  /*should_lock*/ true);
	} else {
		dev_warn(&devdata->spi->dev,
			"Not stopping worker since it appears to be be NULL");
	}

	if (devdata->rf_amp) {
		status = regulator_disable(devdata->rf_amp);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"Failed to disable rf amp power: %d",
					status);
		}
	}

	if (devdata->mcu_core) {
		status = regulator_disable(devdata->mcu_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"Failed to disable MCU power: %d",
					status);
		}
	}

	if (devdata->imu_core) {
		status = regulator_disable(devdata->imu_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"Failed to disable IMU power: %d",
					status);
		}
	}

	if (devdata->mag_core) {
		status = regulator_disable(devdata->mag_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"Failed to disable mag power: %d",
					status);
		}
	}

	if (devdata->hall_sensor) {
		status = regulator_disable(devdata->hall_sensor);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"Failed to disable hall sensor power: %d",
					status);
		}
	}

	/* Release the wakelock so we won't prevent the device from
	 * going to sleep.
	 */
	__pm_relax(&devdata->syncboss_in_use_wake_lock);
}

static void stop_streaming(void *devdata)
{
	stop_streaming_impl((struct syncboss_dev_data *) devdata);
}

static void prox_wake_set(struct syncboss_dev_data *devdata, bool enable)
{
	/* Note: Must be called from syncboss_workqueue context! */
	int status = 0;
	bool must_wake_syncboss = !devdata->is_streaming;

	if (must_wake_syncboss) {
		/* We're about to wake up syncboss, and then shut it
		 * down again.  If we don't silence the prox messages,
		 * the user will get a confusing stream of messages
		 * during this process.
		 */
		devdata->silence_all_prox_events = true;

		start_streaming_impl(devdata, /*force_reset*/true);

		dev_info(&devdata->spi->dev, "Waiting for syncboss to be awake");
		status = wait_for_syncboss_wake_state(devdata, /*awake*/true);
		if (status == 0)
			dev_info(&devdata->spi->dev, "SyncBoss awake");
		else
			goto error;
	}

	push_prox_cal_and_enable_wake(devdata, enable);

	devdata->prox_wake_enabled = enable;

error:
	/* Note: We can stop streaming immediately since the stop
	 * streaming impl sends a sleep command to SyncBoss and waits
	 * for it to sleep.  This means our prox settings are ensured
	 * to get over as well.
	 */
	if (must_wake_syncboss)
		stop_streaming_impl(devdata);

	devdata->silence_all_prox_events = false;
}

static void prox_wake_enable(void *devdata)
{
	prox_wake_set((struct syncboss_dev_data *) devdata, /*enable*/true);
}

static void prox_wake_disable(void *devdata)
{
	prox_wake_set((struct syncboss_dev_data *) devdata, /*enable*/false);
}

static int syncboss_init_pins(struct device *dev)
{
	struct pinctrl *pinctrl = NULL;
	struct pinctrl_state *pins_default = NULL;
	int result = 0;

	pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		dev_err(dev, "Failed to get pin ctrl");
		return -EINVAL;
	}
	pins_default = pinctrl_lookup_state(pinctrl, "syncboss_default");
	if (IS_ERR_OR_NULL(pins_default)) {
		dev_err(dev, "Failed to lookup pinctrl default state");
		return -EINVAL;
	}

	dev_info(dev, "Setting pins to \"syncboss_default\" state");
	result = pinctrl_select_state(pinctrl, pins_default);
	if (result != 0) {
		dev_err(dev, "Failed to set pin state");
		return -EINVAL;
	}
	return 0;
}

static int create_swd_sysfs_symlink(struct device *dev, void *kobj)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd")) {
		return sysfs_create_link((struct kobject *)kobj, &dev->kobj,
					 node->name);
	}

	return 0;
}

static int remove_swd_sysfs_symlink(struct device *dev, void *kobj)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd"))
		sysfs_remove_link((struct kobject *)kobj, node->name);

	return 0;
}

static int syncboss_init_sysfs_attrs(struct syncboss_dev_data *devdata)
{
	struct device *spi_dev = &devdata->spi->dev;
	int ret;

	ret = sysfs_create_group(&spi_dev->kobj, &syncboss_attr_grp);
	if (ret) {
		dev_err(spi_dev, "sysfs_create_group failed: error %d\n", ret);
		return ret;
	}

	ret = sysfs_create_link(&devdata->misc.this_device->kobj,
				&spi_dev->kobj, "spi");
	if (ret) {
		dev_err(spi_dev,
			"sysfs_create_link(spi) failed: error %d\n", ret);
		return ret;
	}

	ret = device_for_each_child(spi_dev, &devdata->misc.this_device->kobj,
				    create_swd_sysfs_symlink);
	if (ret) {
		dev_err(spi_dev,
			"sysfs_create_link(swd) failed: error %d\n", ret);
		return ret;
	}

	return ret;
}

static void syncboss_deinit_sysfs_attrs(struct syncboss_dev_data *devdata)
{
	struct device *spi_dev = &devdata->spi->dev;

	device_for_each_child(spi_dev, &devdata->misc.this_device->kobj,
			      remove_swd_sysfs_symlink);
	sysfs_remove_link(&devdata->misc.this_device->kobj, "spi");
	sysfs_remove_group(&spi_dev->kobj, &syncboss_attr_grp);
}

static irqreturn_t isr_primary_wakeup(int irq, void *p)
{
	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_wakeup(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;

	dev_info(&devdata->spi->dev, "SyncBoss driver wakeup received");

	/* SyncBoss just woke up, so set the last_reset_time_ms */
	devdata->last_reset_time_ms = ktime_get_ms();

	if (devdata->enable_headers) {
		/* Wake up any thread that might be waiting on power state
		 *transitions
		 */
		signal_prox_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_UP,
				  /*should_lock*/ true);
	}

	/* Hold the wake-lock for a short period of time (which should
	 * allow time for clients to open a handle to syncboss)
	 */
	pm_wakeup_event(&devdata->spi->dev, SYNCBOSS_WAKEUP_EVENT_DURATION_MS);
	return IRQ_HANDLED;
}

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;

	atomic_long_set(&devdata->nsync_irq_timestamp, ktime_get_ns());

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_nsync(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;
	struct syncboss_nsync_event event = {
		.timestamp = atomic_long_read(&devdata->nsync_irq_timestamp),
		.count = ++(devdata->nsync_irq_count),
	};

	int status = miscfifo_send_buf(
			&devdata->nsync_fifo,
			(void *)&event,
			sizeof(event));
	if (status < 0)
		dev_warn_ratelimited(
			&devdata->spi->dev,
			"NSYNC fifo send failure: %d\n", status);

	return IRQ_HANDLED;
}

static irqreturn_t timesync_irq_handler(int irq, void *p)
{
	const uint64_t timestamp_ns = ktime_to_ns(ktime_get());
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *) p;

	atomic64_set(&devdata->last_te_timestamp_ns, timestamp_ns);
	return IRQ_HANDLED;
}

static int init_syncboss_dev_data(struct syncboss_dev_data *devdata,
				   struct spi_device *spi)
{
	struct device_node *node = spi->dev.of_node;
	u32 temp_transaction_length = 0;

	devdata->spi = spi;

	devdata->client_count = 0;
	devdata->next_avail_seq_num = 1;
	devdata->transaction_period_ns = SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS;
	devdata->min_time_between_transactions_ns =
		SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS;
	if (!of_property_read_u32(node, "oculus,transaction-length", &temp_transaction_length) &&
	    (temp_transaction_length <= SYNCBOSS_MAX_TRANSACTION_LENGTH))
		devdata->transaction_length = (u16)temp_transaction_length;
	else
		devdata->transaction_length = SYNCBOSS_DEFAULT_TRANSACTION_LENGTH;
	devdata->spi_max_clk_rate = SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE;
	devdata->transaction_ctr = 0;
	devdata->poll_prio = SYNCBOSS_DEFAULT_POLL_PRIO;

	devdata->prox_canc = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdl = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdh = INVALID_PROX_CAL_VALUE;
	devdata->prox_config_version = DEFAULT_PROX_CONFIG_VERSION_VALUE;
	devdata->prox_last_evt = INVALID_PROX_CAL_VALUE;

	devdata->use_fastpath = of_property_read_bool(node, "oculus,syncboss-use-fastpath");

	cpumask_setall(&devdata->cpu_affinity);
	dev_info(&devdata->spi->dev, "Initial SPI thread cpu affinity: %*pb\n",
		cpumask_pr_args(&devdata->cpu_affinity));

	devdata->syncboss_workqueue = create_singlethread_workqueue(
						"syncboss_workqueue");

	devdata->gpio_reset = of_get_named_gpio(node,
			"oculus,syncboss-reset", 0);
	devdata->gpio_timesync = of_get_named_gpio(node,
			"oculus,syncboss-timesync", 0);
	devdata->gpio_wakeup = of_get_named_gpio(node,
			"oculus,syncboss-wakeup", 0);
	devdata->gpio_nsync = of_get_named_gpio(node,
			"oculus,syncboss-nsync", 0);

	if (of_find_property(node, "oculus,syncboss-has-prox", NULL))
		devdata->has_prox = true;

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	if (of_find_property(node,
			"oculus,syncboss-must-enable-cam-temp-sensor-power",
			NULL))
		devdata->must_enable_camera_temp_sensor_power = true;
	dev_info(&devdata->spi->dev, "must-enable-cam-temp-sensor-power: %s",
		 devdata->must_enable_camera_temp_sensor_power ?
		 "true" : "false");
#endif

	dev_info(&devdata->spi->dev,
		 "GPIOs: reset: %d, timesync: %d, wakeup: %d, nsync: %d",
		 devdata->gpio_reset, devdata->gpio_timesync,
		 devdata->gpio_wakeup, devdata->gpio_nsync);

	dev_info(&devdata->spi->dev, "has-prox: %s",
		 devdata->has_prox ? "true" : "false");
	if (devdata->gpio_reset < 0) {
		dev_err(&devdata->spi->dev,
			"The reset GPIO was not specificed in the device tree. We will be unable to reset or update firmware");
	}

	mutex_init(&devdata->queued_buf_mutex);
	mutex_init(&devdata->state_mutex);

	init_waitqueue_head(&devdata->spi_tx_complete);
	return 0;
}

static int syncboss_probe(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata = NULL;
	int status = 0;

	dev_info(&spi->dev, "syncboss device initializing");
	dev_info(
		&spi->dev,
		"name: %s, max speed: %d, cs: %d, bits/word: %d, mode: 0x%x, irq: %d, modalias: %s, cs_gpio: %d",
		dev_name(&spi->dev), spi->max_speed_hz, spi->chip_select,
		spi->bits_per_word, spi->mode, spi->irq, spi->modalias,
		spi->cs_gpio);

	devdata = kzalloc(sizeof(struct syncboss_dev_data), GFP_KERNEL);
	if (!devdata)
		return -ENOMEM;

	devdata->swd_ops.is_busy = is_busy;
	devdata->swd_ops.fw_update_cb = fw_update_cb;

	devdata->tx_bufs[0] = kzalloc(SYNCBOSS_MAX_TRANSACTION_LENGTH * 2,
				      GFP_KERNEL | GFP_DMA);
	devdata->tx_bufs[1] =
		((u8 *)devdata->tx_bufs[0]) + SYNCBOSS_MAX_TRANSACTION_LENGTH;
	if (!devdata->tx_bufs[0]) {
		kfree(devdata);
		return -ENOMEM;
	}

	devdata->rx_buf =
		kzalloc(sizeof(struct rx_history_elem), GFP_KERNEL | GFP_DMA);
	if (!devdata->rx_buf) {
		kfree(devdata->tx_bufs[0]);
		kfree(devdata);
		return -ENOMEM;
	}

	status = init_syncboss_dev_data(devdata, spi);
	if (status < 0)
		return status;

	fw_init_regulator(&spi->dev, &devdata->hall_sensor, "hall-sensor");
	fw_init_regulator(&spi->dev, &devdata->mcu_core, "mcu-core");
	fw_init_regulator(&spi->dev, &devdata->imu_core, "imu-core");
	fw_init_regulator(&spi->dev, &devdata->mag_core, "mag-core");
	fw_init_regulator(&spi->dev, &devdata->rf_amp, "rf-amp");

	dev_set_drvdata(&spi->dev, devdata);

	syncboss_init_pins(&spi->dev);

	/* misc dev stuff */
	devdata->misc.name = SYNCBOSS_DEVICE_NAME;
	devdata->misc.minor = MISC_DYNAMIC_MINOR;
	devdata->misc.fops = &fops;

	status = misc_register(&devdata->misc);
	if (status < 0) {
		dev_err(&spi->dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_alloc;
	}

	devdata->stream_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->stream_fifo.config.header_payload = true;
	devdata->stream_fifo.config.filter_fn = should_send_stream_packet;
	status = miscfifo_register(&devdata->stream_fifo);
	if (status < 0) {
		dev_err(&spi->dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_misc_register;
	}

	devdata->misc_stream.name = SYNCBOSS_STREAM_DEVICE_NAME;
	devdata->misc_stream.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_stream.fops = &stream_fops;

	status = misc_register(&devdata->misc_stream);
	if (status < 0) {
		dev_err(&spi->dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_stream_fifo_register;
	}

	devdata->control_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->control_fifo.config.header_payload = true;
	status = miscfifo_register(&devdata->control_fifo);
	if (status < 0) {
		dev_err(&spi->dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_stream_register;
	}

	devdata->misc_control.name = SYNCBOSS_CONTROL_DEVICE_NAME;
	devdata->misc_control.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_control.fops = &control_fops;

	status = misc_register(&devdata->misc_control);
	if (status < 0) {
		dev_err(&spi->dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_control_fifo_register;
	}

	devdata->prox_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->prox_fifo.config.header_payload = true;
	status = miscfifo_register(&devdata->prox_fifo);
	if (status < 0) {
		dev_err(&spi->dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_control_register;
	}

	devdata->misc_prox.name = SYNCBOSS_PROX_DEVICE_NAME;
	devdata->misc_prox.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_prox.fops = &prox_fops;

	status = misc_register(&devdata->misc_prox);
	if (status < 0) {
		dev_err(&spi->dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_prox_fifo_register;
	}

	/* setup TE timestamp before sysfs nodes get enabled */
	atomic64_set(&devdata->last_te_timestamp_ns, ktime_to_ns(ktime_get()));

	if (devdata->gpio_nsync >= 0) {
		devdata->nsync_fifo.config.kfifo_size = SYNCBOSS_NSYNC_MISCFIFO_SIZE;
		devdata->nsync_fifo.config.header_payload = false;
		status = miscfifo_register(&devdata->nsync_fifo);

		devdata->misc_nsync.name = SYNCBOSS_NSYNC_DEVICE_NAME;
		devdata->misc_nsync.minor = MISC_DYNAMIC_MINOR;
		devdata->misc_nsync.fops = &nsync_fops;

		status = misc_register(&devdata->misc_nsync);
		if (status < 0) {
			dev_err(&spi->dev, "Failed to register misc nsync device, error %d",
					status);
		}
	}

	/*
	 * Create child SWD fw-update devices, if any
	 */
	status = of_platform_populate(spi->dev.of_node, oculus_swd_match_table,
				      NULL, &spi->dev);
	if (status < 0)
		goto error_after_prox_register;

	status = syncboss_init_sysfs_attrs(devdata);
	if (status < 0)
		goto error_after_of_platform_populate;

	devdata->power_state = SYNCBOSS_POWER_STATE_RUNNING;
	devdata->reset_requested = false;
	devdata->enable_headers = true;

	/* Init interrupts */
	if (devdata->gpio_wakeup < 0) {
		dev_err(&spi->dev, "Missing 'oculus,syncboss-wakeup' GPIO");
		status = devdata->gpio_wakeup;
		goto error_after_sysfs;
	}
	devdata->wakeup_irq = gpio_to_irq(devdata->gpio_wakeup);
	irq_set_status_flags(devdata->wakeup_irq, IRQ_DISABLE_UNLAZY);
	/* This irq must be able to wake up the system */
	irq_set_irq_wake(devdata->wakeup_irq, /*on*/ 1);
	status =
		devm_request_threaded_irq(&spi->dev, devdata->wakeup_irq,
					  isr_primary_wakeup, isr_thread_wakeup,
					  IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					  devdata->misc.name, devdata);
	if (status < 0) {
		dev_err(&spi->dev,
			"%s fails to setup syncboss wakeup, error %d", __func__,
			status);
		goto error_after_sysfs;
	}

	if (devdata->gpio_timesync >= 0) {
		devdata->timesync_irq = gpio_to_irq(devdata->gpio_timesync);
		status = devm_request_irq(&spi->dev, devdata->timesync_irq,
				timesync_irq_handler,
				IRQF_ONESHOT | IRQF_TRIGGER_RISING,
				devdata->misc.name, devdata);
		if (status < 0) {
			dev_err(&spi->dev,
				"%s fails to setup syncboss timesync, error %d",
				__func__, status);
			goto error_after_sysfs;
		}
	}

	if (devdata->gpio_nsync >= 0) {
		devdata->nsync_irq_count = 0;
		devdata->nsync_irq = gpio_to_irq(devdata->gpio_nsync);
		irq_set_status_flags(devdata->nsync_irq, IRQ_DISABLE_UNLAZY);
		status = devm_request_threaded_irq(
			&spi->dev, devdata->nsync_irq, isr_primary_nsync,
			isr_thread_nsync, IRQF_ONESHOT | IRQF_TRIGGER_RISING,
			devdata->misc_nsync.name, devdata);
	}

	/* Init device as a wakeup source (so prox interrupts can hold
	 * the wake lock)
	 */
	device_init_wakeup(&spi->dev, /*enable*/ true);

	wakeup_source_init(&devdata->syncboss_in_use_wake_lock, "syncboss");

	/* This looks kinda hacky, but starting and then stopping the stream is
	 * the simplest way to get the SyncBoss in an initial sleep state
	 */
	reset_syncboss(devdata);
	return 0;

error_after_sysfs:
	syncboss_deinit_sysfs_attrs(devdata);
error_after_of_platform_populate:
	of_platform_depopulate(&spi->dev);
error_after_prox_register:
	misc_deregister(&devdata->misc_prox);
error_after_prox_fifo_register:
	miscfifo_destroy(&devdata->prox_fifo);
error_after_control_register:
	misc_deregister(&devdata->misc_control);
error_after_control_fifo_register:
	miscfifo_destroy(&devdata->control_fifo);
error_after_stream_register:
	misc_deregister(&devdata->misc_stream);
error_after_stream_fifo_register:
	miscfifo_destroy(&devdata->stream_fifo);
error_after_misc_register:
	misc_deregister(&devdata->misc);
error_after_alloc:
	kfree(devdata->rx_buf);
	kfree(devdata->tx_bufs[0]);

	flush_workqueue(devdata->syncboss_workqueue);
	destroy_workqueue(devdata->syncboss_workqueue);

	kfree(devdata);
	return status;
}

static int syncboss_remove(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata = NULL;

	devdata = (struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);

	of_platform_depopulate(&spi->dev);

	regulator_put(devdata->hall_sensor);
	regulator_put(devdata->mcu_core);
	regulator_put(devdata->imu_core);
	regulator_put(devdata->mag_core);
	regulator_put(devdata->rf_amp);

	syncboss_deinit_sysfs_attrs(devdata);

	misc_deregister(&devdata->misc_stream);
	misc_deregister(&devdata->misc_control);
	misc_deregister(&devdata->misc_prox);
	misc_deregister(&devdata->misc);

	miscfifo_destroy(&devdata->stream_fifo);
	miscfifo_destroy(&devdata->control_fifo);
	miscfifo_destroy(&devdata->prox_fifo);

	kfree(devdata->tx_bufs[0]);
	kfree(devdata->rx_buf);

	flush_workqueue(devdata->syncboss_workqueue);
	destroy_workqueue(devdata->syncboss_workqueue);

	kfree(devdata);
	return 0;
}

static void syncboss_on_camera_probe(struct syncboss_dev_data *devdata)
{
	dev_info(&devdata->spi->dev, "Turning on cameras");

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	enable_cameras();
#endif
	devdata->cameras_enabled = true;
}

static void syncboss_snoop_write(struct syncboss_dev_data *devdata,
				 const struct spi_data *packet, size_t size)
{
	if (size < sizeof(struct spi_data)) {
		dev_warn(&devdata->spi->dev, "SPI data packet smaller than expected");
		return;
	}

	switch (packet->type) {
	case SYNCBOSS_CAMERA_PROBE_MESSAGE_TYPE:
		syncboss_on_camera_probe(devdata);
		break;
	case SYNCBOSS_CAMERA_RELEASE_MESSAGE_TYPE:
		syncboss_on_camera_release(devdata);
		break;
	default:
		/* Nothing to do for this message type */
		break;
	}
}

static int queue_packet_with_timeout(struct syncboss_dev_data *devdata,
				     struct spi_data *packet, size_t size)
{
	int status = 0;
	int wait_status = 0;

	status = queue_tx_packet(devdata, packet, size);
	if (status == 0)
		/* Normal success case */
		return status;

	if (status == -ENOBUFS) {
		/* Wait for up to SYNCBOSS_WRITE_STAGE_TIMEOUT_MS for
		 * buffer space to become available
		 */

		dev_dbg(&devdata->spi->dev,
			"Write buffer full, waiting for a bit..");

		/* Wait outside of the lock */
		mutex_unlock(&devdata->state_mutex);
		wait_status = wait_event_interruptible_timeout(
				devdata->spi_tx_complete,
				(queue_tx_packet(devdata, packet, size) == 0),
				msecs_to_jiffies(
					SYNCBOSS_WRITE_STAGE_TIMEOUT_MS));
		status = mutex_lock_interruptible(&devdata->state_mutex);
		if (status != 0) {
			dev_err(&devdata->spi->dev,
				"Failed to get state lock with error %d",
				status);
			return status;
		}

		if (wait_status > 0) {
			dev_dbg(&devdata->spi->dev,
				"Write successful after wait");
			return 0;
		} else if (wait_status == 0) {
			dev_err(&devdata->spi->dev,
				"Write timed out after wait");
			return -ETIMEDOUT;
		} else {
			dev_err(&devdata->spi->dev,
				"Write failed after wait with error %d",
				wait_status);
			return wait_status;
		}
	} else {
		/* Not a recoverable error */
		return status;
	}
}

static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	int status = 0;
	struct syncboss_dev_data *devdata =
		container_of(filp->private_data, struct syncboss_dev_data,
			     misc);
	u8 packet_buf[SYNCBOSS_MAX_TRANSACTION_LENGTH] = {0};
	struct spi_data* packet = (struct spi_data *)packet_buf;

	if (count > SYNCBOSS_MAX_TRANSACTION_LENGTH ||
		copy_from_user(packet, (void *)buf, count)) {
		dev_err(&devdata->spi->dev,
			"count is too large or copy_from_user failed!\n");
		return -ENOBUFS;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"Failed to get state lock with error %d",
			status);
		return status;
	}

	syncboss_snoop_write(devdata, packet, count);
	status = queue_packet_with_timeout(devdata, packet, count);
	if (status != 0)
		goto error;

	/* In the success case, we're supposed to return the number of
	 * bytes successfully written (which would be all of them)
	 */
	status = count;

error:
	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t store_reset(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	bool should_reset = false;
	int status = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	if ((count >= 1) && (strtobool(buf, &should_reset) == 0) &&
	    should_reset) {

		status = mutex_lock_interruptible(&devdata->state_mutex);
		if (status != 0) {
			dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
				status);
			return status;
		}
		syncboss_reset(devdata);
		mutex_unlock(&devdata->state_mutex);

		status = count;
	} else {
		dev_err(dev, "Invalid argument: \"%s\".	 Must be 1", buf);
		status = -EINVAL;
	}
	return status;
}

static ssize_t show_transaction_length(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->transaction_length);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_transaction_length(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int status = 0;
	u32 temp_transaction_length = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_transaction_length);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	} else if (temp_transaction_length > SYNCBOSS_MAX_TRANSACTION_LENGTH) {
		dev_err(dev, "Transaction length must be <= %d",
			SYNCBOSS_MAX_TRANSACTION_LENGTH);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->transaction_length = (u16)temp_transaction_length;

	if (devdata->is_streaming) {
		dev_info(dev,
			"Transaction length changed while streaming.");
		dev_info(dev,
			"This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static int next_seq_num(int current_seq)
{
	int next_seq = current_seq + 1;
	BUG_ON((current_seq < SYNCBOSS_MIN_SEQ_NUM) ||
	       (current_seq > SYNCBOSS_MAX_SEQ_NUM));

	if (next_seq > SYNCBOSS_MAX_SEQ_NUM)
		next_seq = SYNCBOSS_MIN_SEQ_NUM;
	return next_seq;
}

static ssize_t show_next_avail_seq_num(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n",
			   devdata->next_avail_seq_num);

	devdata->next_avail_seq_num = next_seq_num(devdata->next_avail_seq_num);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t show_cpu_affinity(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%*pb\n",
		cpumask_pr_args(&devdata->cpu_affinity));

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_cpu_affinity(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	int status = 0;
	struct cpumask temp_cpu_affinity;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = cpumask_parse(buf, &temp_cpu_affinity);
	if (status < 0) {
		dev_err(dev, "Failed to parse cpumask out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->cpu_affinity = temp_cpu_affinity;

	if (devdata->is_streaming) {
		dev_info(dev,
			"CPU affinity changed while streaming.");
		dev_info(dev,
			"This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static ssize_t show_transaction_period_us(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n",
			   devdata->transaction_period_ns / 1000);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_transaction_period_us(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	int status = 0;
	u32 temp_transaction_period_us = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_transaction_period_us);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->transaction_period_ns = temp_transaction_period_us * 1000;
	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Transaction period changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_minimum_time_between_transactions_us(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(
	    buf, PAGE_SIZE, "%ld\n",
	    (devdata->min_time_between_transactions_ns / NSEC_PER_USEC));

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_minimum_time_between_transactions_us(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int status = 0;
	u32 temp_minimum_time_between_transactions_us = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10,
			   &temp_minimum_time_between_transactions_us);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->min_time_between_transactions_ns =
	    temp_minimum_time_between_transactions_us * NSEC_PER_USEC;
	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Minimum time between transactions changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_power(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%u\n", devdata->power_state);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}


static ssize_t show_stats(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE,
			   "num_bad_magic_numbers\t"
			   "num_bad_checksums\t"
			   "num_rejected_transactions\n"
			   "%u\t%u\t%u\n",
			   devdata->stats.num_bad_magic_numbers,
			   devdata->stats.num_bad_checksums,
			   devdata->stats.num_rejected_transactions);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t show_spi_max_clk_rate(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->spi_max_clk_rate);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_spi_max_clk_rate(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int status = 0;
	u32 temp_spi_max_clk_rate = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base */10, &temp_spi_max_clk_rate);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	if (temp_spi_max_clk_rate > devdata->spi->max_speed_hz) {
		dev_err(dev, "Invalid value of spi_max_clk_rate: %u (max value: %u)",
			temp_spi_max_clk_rate, devdata->spi->max_speed_hz);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	/* 0 is shorthand for the max rate */
	if (temp_spi_max_clk_rate == 0)
		devdata->spi_max_clk_rate = devdata->spi->max_speed_hz;
	else
		devdata->spi_max_clk_rate = temp_spi_max_clk_rate;

	status = count;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "SPI max clock rate changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return status;
}

static ssize_t show_poll_priority(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", devdata->poll_prio);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_poll_priority(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int status = 0;
	u16 temp_priority = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou16(buf, /*base */10, &temp_priority);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	} else if (temp_priority < 1 ||
		   temp_priority > (MAX_USER_RT_PRIO -1)) {
		dev_err(dev, "Invalid real time priority");
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->poll_prio = temp_priority;

	if (devdata->is_streaming) {
		dev_info(dev,
			 "Poll thread priority changed while streaming.");
		dev_info(dev,
			 "This change will not take effect until the stream is stopped and restarted");
	}

	mutex_unlock(&devdata->state_mutex);
	return count;
}

static ssize_t show_te_timestamp(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct syncboss_dev_data *devdata =
	(struct syncboss_dev_data *) dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
		atomic64_read(&devdata->last_te_timestamp_ns));
}

static ssize_t store_power(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	dev_err(dev, "Power file writing deprecated.  This is a no-op");
	return 0;
}

static ssize_t show_num_cameras(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int num_cams = 0;

#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
	num_cams = get_num_cameras();
#endif

	return scnprintf(buf, PAGE_SIZE, "%d\n", num_cams);
}

static ssize_t show_enable_fastpath(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	int status = 0;
	int retval = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	retval = scnprintf(buf, PAGE_SIZE, "%d\n", !!devdata->use_fastpath);

	mutex_unlock(&devdata->state_mutex);
	return retval;
}

static ssize_t store_enable_fastpath(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int status = 0;
	u32 temp_fastpath = 0;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	status = kstrtou32(buf, /*base*/10, &temp_fastpath);
	if (status < 0) {
		dev_err(dev, "Failed to parse integer out of %s", buf);
		return -EINVAL;
	}

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		/* Failed to get the sem */
		dev_err(&devdata->spi->dev, "Failed to get state mutex: %d",
			status);
		return status;
	}

	devdata->use_fastpath = !!temp_fastpath;
	status = count;

	mutex_unlock(&devdata->state_mutex);
	return status;
}

/*SPI Driver Info */
struct spi_driver oculus_syncboss_driver = {
	.driver = {
		.name = "oculus_syncboss",
		.owner = THIS_MODULE,
		.of_match_table = oculus_syncboss_table,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS
	},
	.probe	= syncboss_probe,
	.remove = syncboss_remove
};

static int __init syncboss_init(void)
{
	return spi_register_driver(&oculus_syncboss_driver);
}

static void __exit syncboss_exit(void)
{
	spi_unregister_driver(&oculus_syncboss_driver);
}

module_init(syncboss_init);
module_exit(syncboss_exit);
MODULE_DESCRIPTION("SYNCBOSS");
MODULE_LICENSE("GPL v2");
