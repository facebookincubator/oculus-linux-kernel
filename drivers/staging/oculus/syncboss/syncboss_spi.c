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
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/wait.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_SYNCBOSS_CAMERA_CONTROL
#include <syncboss_camera.h>
#endif

#include "fw_helpers.h"

#include "spi_fastpath.h"
#include "syncboss.h"
#include "syncboss_protocol.h"

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
 * Fastpath Notes
 * ==============
 *
 * Fastpath will not work on all devices!
 *
 * The fastpath feature of this drive, when enabled, makes the
 * following major assumptions about the implementation of the
 * SPI driver, which are not true in general or with many SPI
 * drivers.
 *  1) A single transaction can be sent more than once, and
 *     without un-preparing the re-preparing each time. This is
 *     not true for SPI drivers that use prepare/unprepare to
 *     map and unmap DMA buffers with APIs like dma_map_single().
 *     Buffers mapped with dma_map_single() cannot be safely
 *     accessed by the CPU until after dma_unmap_single() is
 *     called or dma_sync_single_for_{cpu,device}() are used
 *     to transfer ownership between the CPUs and the peripheral.
 *  2) More than one message can be prepared at a time. The SPI
 *     framework makes no such guarantee.
 */

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

/* The amount of time we should hold the reset line low when doing a reset. */
#define SYNCBOSS_RESET_TIME_MS 5

#define SPI_DATA_MAGIC_NUM 0xDEFEC8ED

#define SPI_DATA_REJECTED_MAGIC_NUM 0xCACACACA

#define SYNCBOSS_MISCFIFO_SIZE 1024

#define SYNCBOSS_NSYNC_MISCFIFO_SIZE 256

#define SYNCBOSS_DEFAULT_POLL_PRIO 52

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

/*
 * Maximum pending messages enqueued by syncboss_write()
 */
#define MAX_MSG_QUEUE_ITEMS 100

/* Power states the SyncBoss MCU can be in.  Note that these were
 * taken from the firmware's syncboss_registers.h and must match those
 * values.
 */
#define SYNCBOSS_POWER_STATE_RUNNING 0
#define SYNCBOSS_POWER_STATE_OFF 2

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

static void syncboss_on_camera_probe(struct syncboss_dev_data *devdata);
static void syncboss_on_camera_release(struct syncboss_dev_data *devdata);
static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos);

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
	const struct syncboss_data *packet = (struct syncboss_data *)payload;

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

static int start_streaming_locked(struct syncboss_dev_data *devdata, bool force_reset);
static void stop_streaming_locked(struct syncboss_dev_data *devdata);
static int wait_for_syncboss_wake_state(struct syncboss_dev_data *devdata,
					bool state);

static void reset_syncboss(struct syncboss_dev_data *devdata)
{
	int status = 0;

	/*
	 * Start then stop the stream to pin-reset syncboss and tell it to
	 * sleep
	 */
	mutex_lock(&devdata->state_mutex);
	status = start_streaming_locked(devdata, true);
	if (!status) {
		stop_streaming_locked(devdata);
	} else {
		dev_err(&devdata->spi->dev, "Failed to start streaming from reset_syncboss (%d)",
			status);
	}
	mutex_unlock(&devdata->state_mutex);
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

	if (devdata->boots_to_shutdown_state)
		syncboss_pin_reset(devdata);
	else
		reset_syncboss(devdata);
}

static int syncboss_inc_client_count_locked(struct syncboss_dev_data *devdata)
{
	int status = 0;

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

	if (devdata->client_count == 0) {
		dev_info(&devdata->spi->dev, "Starting streaming thread work");
		status = start_streaming_locked(devdata, true);
	}

	if (!status)
		++devdata->client_count;

	return status;
}

static int syncboss_dec_client_count_locked(struct syncboss_dev_data *devdata)
{
	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->client_count < 1);

	--devdata->client_count;

	if (devdata->client_count == 0) {
		dev_info(&devdata->spi->dev, "Stopping streaming thread work");
		stop_streaming_locked(devdata);
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

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status)
	        return status;

	/* Ensure a FW update is not in progress */
	status = device_for_each_child(&devdata->spi->dev, NULL,
				       fw_update_check_busy);
	if (status)
		goto out;

	status = syncboss_inc_client_count_locked(devdata);

out:
	mutex_unlock(&devdata->state_mutex);

	return status;
}

static int signal_prox_event_locked(struct syncboss_dev_data *devdata, int evt)
{
	int status = 0;
	bool should_update_last_evt = true;
	struct syncboss_driver_data_header_driver_message_t msg_to_send;

	BUG_ON(!mutex_is_locked(&devdata->state_mutex));

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

	return status;
}

static void signal_prox_event(struct syncboss_dev_data *devdata, int evt)
{
	mutex_lock(&devdata->state_mutex);
	signal_prox_event_locked(devdata, evt);
	mutex_unlock(&devdata->state_mutex);
}

struct prox_work_data {
	struct work_struct work;
	struct syncboss_dev_data *devdata;
	int evt;
};

static void do_signal_prox_event(struct work_struct *work)
{
	struct prox_work_data *wd = container_of(work, struct prox_work_data, work);

	signal_prox_event(wd->devdata, wd->evt);

	kfree(wd);
}

static void queue_prox_event(struct syncboss_dev_data *devdata, int evt)
{
	struct prox_work_data *wd = kmalloc(sizeof(*wd), GFP_KERNEL);
	if (!wd)
		return;

	wd->devdata = devdata;
	wd->evt = evt;

	INIT_WORK(&wd->work, do_signal_prox_event);
	queue_work(devdata->syncboss_workqueue, &wd->work);
}

static void prox_wake_enable_locked(struct syncboss_dev_data *devdata);
static void prox_wake_disable_locked(struct syncboss_dev_data *devdata);

static int syncboss_prox_open(struct inode *inode, struct file *f)
{
	int status;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_prox);
	dev_info(&devdata->spi->dev, "SyncBoss prox handle opened (%s)",
		 current->comm);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status)
	        return status;

	status = miscfifo_fop_open(f, &devdata->prox_fifo);
	if (status)
		goto out;

	++devdata->prox_client_count;

	/* Send the last prox_event */
	if (devdata->prox_last_evt != INVALID_PROX_CAL_VALUE) {
		dev_info(&devdata->spi->dev,
			 "Signaling prox_last_evt (%d)",
			 devdata->prox_last_evt);
		signal_prox_event_locked(devdata, devdata->prox_last_evt);
	}

	if (devdata->prox_client_count == 1) {
		if (devdata->has_prox)
			read_prox_cal(devdata);

		/* Enable prox wakeup */
		dev_info(&devdata->spi->dev, "Enabling prox wakeup");
		prox_wake_enable_locked(devdata);
	}

	mutex_unlock(&devdata->state_mutex);
out:
	return status;
}

static int syncboss_prox_release(struct inode *inode, struct file *file)
{
	int status;
	struct miscfifo_client *client = file->private_data;
	struct miscfifo *mf = client->mf;
	struct syncboss_dev_data *devdata =
		container_of(mf, struct syncboss_dev_data,
			     prox_fifo);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status)
		return status;

	--devdata->prox_client_count;

	if (devdata->prox_client_count == 0) {
		/* Disable prox wakeup */
		dev_info(&devdata->spi->dev, "Disabling prox wakeup");
		prox_wake_disable_locked(devdata);
	}

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

static int syncboss_set_stream_type_filter(
	struct syncboss_dev_data *devdata,
	struct file *file,
	const struct syncboss_driver_stream_type_filter __user *filter)
{
	int status;
	struct device *dev = &devdata->spi->dev;
	struct syncboss_driver_stream_type_filter *new_filter = NULL;
	struct syncboss_driver_stream_type_filter *existing_filter =
		miscfifo_fop_get_context(file);

	if (existing_filter) {
		miscfifo_fop_set_context(file, NULL);
		devm_kfree(dev, existing_filter);
	}

	new_filter = devm_kzalloc(dev, sizeof(*new_filter), GFP_KERNEL);
	if (!new_filter)
		return -ENOMEM;

	status = copy_from_user(new_filter, filter, sizeof(*new_filter));
	if (status != 0) {
		dev_err(dev, "Failed to copy %d bytes from user stream filter\n",
			status);
		return -EFAULT;
	}

	/* Sanity check new_filter */
	if (new_filter->num_selected > SYNCBOSS_MAX_FILTERED_TYPES) {
		dev_err(dev, "Sanity check of user stream filter failed (num_selected = %d)\n",
			new_filter->num_selected);
		return -EINVAL;
	}

	miscfifo_fop_set_context(file, new_filter);
	return 0;
}

static long syncboss_stream_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	struct miscfifo_client *client = file->private_data;
	struct syncboss_dev_data *devdata =
		container_of(client->mf, struct syncboss_dev_data, stream_fifo);

	switch (cmd) {
	case SYNCBOSS_SET_STREAMFILTER_IOCTL:
		return syncboss_set_stream_type_filter(devdata, file,
			(struct syncboss_driver_stream_type_filter *)arg);
	default:
		dev_err(&devdata->spi->dev, "Unrecognized IOCTL %ul\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int syncboss_stream_release(struct inode *inode, struct file *file)
{
	int status = 0;
	struct miscfifo_client *client = file->private_data;
	struct syncboss_dev_data *devdata =
		container_of(client->mf, struct syncboss_dev_data, stream_fifo);
	struct syncboss_driver_stream_type_filter *stream_type_filter = NULL;

	stream_type_filter = miscfifo_fop_get_context(file);
	if (stream_type_filter) {
		miscfifo_fop_set_context(file, NULL);
		devm_kfree(&devdata->spi->dev, stream_type_filter);
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

void syncboss_pin_reset(struct syncboss_dev_data *devdata)
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
	int status;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data, misc);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status)
		return status;

	syncboss_dec_client_count_locked(devdata);
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

static inline u8 calculate_checksum(const struct syncboss_transaction *trans, size_t len)
{
	const u8 *buf = (u8*)trans;

	u8 x = 0, sum = 0;
	for (x = 0; x < len; ++x)
		sum += buf[x];
	return 0 - sum;
}

static void syncboss_snoop_write(struct syncboss_dev_data *devdata,
				 const struct syncboss_transaction *tx)
{
	const void *tx_end = tx + sizeof(*tx);
	const void *data = &tx->data.syncboss_data;
	const struct syncboss_data *packet;

	/* Snoop each packet in the transaction */
	while (data + sizeof(*packet) < tx_end) {
		packet = (struct syncboss_data *)data;

		if (packet->type == 0)
			break;

		switch (packet->type)
		{
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
		data += (sizeof(*packet) + packet->data_len);
	}
}

static unsigned long copy_buffer(void *dest, const void *src, size_t count, bool from_user)
{
	if (from_user) {
		return copy_from_user(dest, src, count);
	} else {
		memcpy(dest, src, count);
		return 0;
	}
}

static ssize_t queue_tx_packet(struct syncboss_dev_data *devdata, const void *buf,
                           size_t count, bool from_user)
{
	int status = 0;
	struct syncboss_msg *smsg = NULL;
	struct spi_transfer *spi_xfer = NULL;

	if (count > MAX_TRANSACTION_DATA_LENGTH) {
		dev_err(&devdata->spi->dev, "write is larger than max transaction length!\n");
		return -EINVAL;
	}

	/*
	 * Combine this command in the same transaction as the previous one
	 * if there is room.
	 */
	mutex_lock(&devdata->msg_queue_lock);
	if (!list_empty(&devdata->msg_queue_list)) {
		smsg = list_last_entry(&devdata->msg_queue_list, struct syncboss_msg, list);
		if (!smsg->finalized &&
		    smsg->transaction_bytes + count <= devdata->transaction_length) {
			void *dest = smsg->tx.data.raw_data + smsg->transaction_bytes
					- sizeof(struct transaction_header);
			if (copy_buffer(dest, buf, count, from_user)) {
				dev_err(&devdata->spi->dev, "copy_buffer to existing smsg failed!\n");
				mutex_unlock(&devdata->msg_queue_lock);
				return -EINVAL;
			}
			smsg->transaction_bytes += count;
			mutex_unlock(&devdata->msg_queue_lock);
			return count;
		}
		smsg = NULL;
	}
	mutex_unlock(&devdata->msg_queue_lock);

	// If we're here, we can't combine transactions. Create a new one, in its own message.
	if (devdata->msg_queue_item_count > MAX_MSG_QUEUE_ITEMS) {
		dev_warn_ratelimited(&devdata->spi->dev, "msg queue is full\n");
		status = -EBUSY;
		goto err;
	}

	smsg = kzalloc(sizeof(*smsg), GFP_KERNEL | GFP_DMA);
	if (!smsg) {
		dev_err(&devdata->spi->dev, "syncboss_msg allocation failed!\n");
		status = -ENOMEM;
		goto err;
	}

	if (copy_buffer(&smsg->tx.data.raw_data, buf, count, from_user)) {
		dev_err(&devdata->spi->dev, "copy_buffer failed!\n");
		status = -EINVAL;
		goto err;
	}
	smsg->transaction_bytes = sizeof(smsg->tx.header) + count;
	smsg->tx.header.magic_num = SPI_DATA_MAGIC_NUM;

	spi_xfer = &smsg->spi_xfer;
	spi_xfer->tx_buf = &smsg->tx;
	spi_xfer->rx_buf = devdata->rx_elem->buf;
	spi_xfer->len = devdata->transaction_length;
	spi_xfer->bits_per_word = 8;
	spi_xfer->speed_hz = devdata->spi_max_clk_rate;

	spi_message_init(&smsg->spi_msg);
	spi_message_add_tail(spi_xfer, &smsg->spi_msg);
	smsg->spi_msg.spi = devdata->spi;

	// Enqueue the new message
	mutex_lock(&devdata->msg_queue_lock);
	// Check the count again, this time  under the lock in case it has changed.
	if (devdata->msg_queue_item_count > MAX_MSG_QUEUE_ITEMS) {
		dev_warn(&devdata->spi->dev, "msg queue is now full\n");
		status = -EBUSY;
		goto err_locked;
	}
	list_add_tail(&smsg->list, &devdata->msg_queue_list);
	devdata->msg_queue_item_count++;

	// Wake the kthread if one has been started. If streaming is stopped it may be asleep.
	if (devdata->worker)
		wake_up_process(devdata->worker);

	mutex_unlock(&devdata->msg_queue_lock);

	return count;

err_locked:
	mutex_unlock(&devdata->msg_queue_lock);
err:
	kfree(smsg);
	return status;
}

static int distribute_packet(struct syncboss_dev_data *devdata,
			     const struct syncboss_data *packet)
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
			queue_prox_event(devdata,
					  prox_state ?
					  SYNCBOSS_PROX_EVENT_PROX_ON :
					  SYNCBOSS_PROX_EVENT_PROX_OFF);
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

static bool any_data_in_spi_trans(const char *buf)
{
	const struct syncboss_transaction *trans = (struct syncboss_transaction *)buf;
	/*
	 * return true if there's any data in the packet (other than the magic
	 * number and checksum)
	 */
	return trans->data.type != 0;
}

static bool recent_reset_event(struct syncboss_dev_data *devdata)
{
	s64 current_time_ms = ktime_get_ms();

	if ((current_time_ms - devdata->last_reset_time_ms) <=
	    SYNCBOSS_RESET_SPI_SETTLING_TIME_MS)
		return true;
	return false;
}

static bool spi_nrf_sanity_check_trans(struct syncboss_dev_data *devdata)
{
	const struct syncboss_transaction *trans = (struct syncboss_transaction *)devdata->rx_elem->buf;

	if (trans->header.magic_num != SPI_DATA_MAGIC_NUM) {
		/* To avoid noise in the log, only log an error if we
		 * haven't recently reset the mcu
		 */
		if (!recent_reset_event(devdata))
			dev_err_ratelimited(&devdata->spi->dev,
					    "Bad magic number detected: 0x%08x",
					    trans->header.magic_num);
		++devdata->stats.num_bad_magic_numbers;
		return false;
	}

	if (calculate_checksum(trans, devdata->transaction_length) != 0) {
		dev_err_ratelimited(&devdata->spi->dev, "Bad checksum detected");
		++devdata->stats.num_bad_checksums;
		return false;
	}

	/* If we've made it this far, the magic number and checksum
	 * look good, so this is most likely valid data.
	 */
	return true;
}

static void process_rx_data(struct syncboss_dev_data *devdata,
			    u64 prev_transaction_start_time_ns,
			    u64 prev_transaction_end_time_ns)
{
	struct rx_history_elem *rx_elem = devdata->rx_elem;
	const struct syncboss_transaction *trans = (struct syncboss_transaction *)rx_elem->buf;
	const struct syncboss_data *current_packet = &trans->data.syncboss_data;
	const uint8_t *trans_end = (uint8_t *)trans + devdata->transaction_length;

	/*
	 * See "logging notes" section at the top of
	 * this file
	 */
#if defined(CONFIG_DYNAMIC_DEBUG)
	print_hex_dump_bytes("syncboss received: ",
			     DUMP_PREFIX_OFFSET,
			     rx_elem->buf,
			     devdata->transaction_length);
#endif

	rx_elem->rx_ctr = ++devdata->transaction_ctr;
	rx_elem->transaction_start_time_ns =
		prev_transaction_start_time_ns;
	rx_elem->transaction_end_time_ns =
		prev_transaction_end_time_ns;
	rx_elem->transaction_length = devdata->transaction_length;

	/* Iterate over the packets and distribute them to either the
	 * stream or control fifos
	 */
	while (current_packet->type != 0) {
		/* Sanity check that the packet is entirely contained
		 * within the rx buffer and doesn't overflow
		 */
		const uint8_t *pkt_end = current_packet->data + current_packet->data_len;
		if (pkt_end > trans_end) {
			dev_err_ratelimited(&devdata->spi->dev, "Data packet overflow");
			return;
		}
		distribute_packet(devdata, current_packet);
		/* Next packet */
		current_packet = (struct syncboss_data *)pkt_end;
	}
}

static int syncboss_spi_transfer_thread(void *ptr)
{
	int status;
	struct spi_device *spi = (struct spi_device *)ptr;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);
	struct syncboss_msg *prepared_smsg = NULL;
	u64 prev_transaction_start_time_ns = 0;
	u64 prev_transaction_end_time_ns = 0;
	u32 spi_max_clk_rate = 0;
	u16 transaction_length = 0;
	int transaction_period_ns = 0;
	int min_time_between_transactions_ns = 0;
	bool headers_enabled = false;
	bool use_fastpath;
	struct sched_param sched_settings = {
		.sched_priority = devdata->poll_prio
	};

	status = sched_setscheduler(devdata->worker, SCHED_FIFO, &sched_settings);
	if (status) {
		dev_warn(&spi->dev, "Failed to set SCHED_FIFO. (%d)",
			 status);
	}

	spi_max_clk_rate = devdata->spi_max_clk_rate;
	transaction_length = devdata->transaction_length;
	transaction_period_ns = devdata->transaction_period_ns;
	min_time_between_transactions_ns =
		devdata->min_time_between_transactions_ns;
	headers_enabled = devdata->enable_headers;
	use_fastpath = devdata->use_fastpath;

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
		use_fastpath ? "yes" : "no");

	while (!kthread_should_stop()) {
		struct syncboss_msg *smsg;
		bool first_attempt;
		u64 time_to_wait_us = 0;

		mutex_lock(&devdata->msg_queue_lock);
		smsg = list_first_entry_or_null(&devdata->msg_queue_list, struct syncboss_msg, list);
		if (smsg) {
			first_attempt = !smsg->finalized;
		} else {
			smsg = devdata->default_smsg;
			first_attempt = false;
		}
		smsg->finalized = true;
		mutex_unlock(&devdata->msg_queue_lock);

		/*
		 * If streaming is stopped and there's no queued message to send,
		 * go to sleep until something is queued or this thread is asked to
		 * stop.
		 */
		if (!devdata->is_streaming && smsg == devdata->default_smsg) {
			set_current_state(TASK_INTERRUPTIBLE);
			schedule();
			continue;
		}

		smsg->spi_xfer.speed_hz = spi_max_clk_rate;
		smsg->spi_xfer.len = transaction_length;
		if (first_attempt) {
			smsg->tx.header.checksum = calculate_checksum(&smsg->tx,
								      transaction_length);
			syncboss_snoop_write(devdata, &smsg->tx);
		}

		if (use_fastpath && smsg != devdata->default_smsg) {
			if (prepared_smsg && smsg != prepared_smsg) {
				devdata->spi_prepare_ops.unprepare_message(spi->master, &prepared_smsg->spi_msg);
				prepared_smsg = NULL;
			}

			if (!prepared_smsg) {
				devdata->spi_prepare_ops.prepare_message(spi->master, &smsg->spi_msg);
				prepared_smsg = smsg;
			}
			spi->master->cur_msg = &smsg->spi_msg;
		}

		/*
		 * Check if we have to wait a bit before initiating the next SPI
		 * transaction
		 */
		if (prev_transaction_start_time_ns != 0) {
			u64 period_requirement_ns = 0;
			u64 setup_requirement_ns = 0;
			u64 now_ns = ktime_get_ns();

			u64 time_since_prev_transaction_start_ns =
				now_ns -
				prev_transaction_start_time_ns;
			u64 time_since_prev_transaction_end_ns =
				now_ns -
				prev_transaction_end_time_ns;

			if (time_since_prev_transaction_start_ns < transaction_period_ns) {
				period_requirement_ns =
					transaction_period_ns -
					time_since_prev_transaction_start_ns;
			}

			if (time_since_prev_transaction_end_ns < min_time_between_transactions_ns) {
				setup_requirement_ns =
					min_time_between_transactions_ns -
					time_since_prev_transaction_end_ns;
			}

			time_to_wait_us = max(period_requirement_ns, setup_requirement_ns) / NSEC_PER_USEC;
		}

		if (time_to_wait_us > 0) {
			if (time_to_wait_us < 10) {
				/*
				 * Delays of less than about 10us are not worth
				 * a context switch.
				 */
				udelay(time_to_wait_us);
			} else {
				/*
				 * For longer delays sleep. Use usleep_range() to
				 * allow wake-ups to be coalesced.
				 */
				usleep_range(time_to_wait_us - SYNCBOSS_SCHEDULING_SLOP_US,
					     time_to_wait_us + SYNCBOSS_SCHEDULING_SLOP_US);
			}
		}

#if defined(CONFIG_DYNAMIC_DEBUG)
		if (smsg != devdata->default_smsg) {
			/* See "logging notes" section at the top of this
			 * file
			 */
			print_hex_dump_bytes("syncboss sending: ",
					    DUMP_PREFIX_OFFSET, &smsg->tx,
					    transaction_length);
		}
#endif

		prev_transaction_start_time_ns = ktime_get_ns();
		if (devdata->use_fastpath)
			status = spi_fastpath_transfer(spi, &smsg->spi_msg);
		else
			status = spi_sync(spi, &smsg->spi_msg);
		prev_transaction_end_time_ns = ktime_get_ns();

		if (status != 0) {
			dev_err(&spi->dev, "spi_sync failed with error %d",
				status);

			if (status == -ETIMEDOUT) {
				/* Retry if needed and keep going */
				continue;
			}

			break;
		} else if (spi_nrf_sanity_check_trans(devdata)) {
			if (any_data_in_spi_trans(devdata->rx_elem->buf)) {
				process_rx_data(devdata,
						prev_transaction_start_time_ns,
						prev_transaction_end_time_ns);
			}

			// Remove the completed message from the queue.
			if (smsg != devdata->default_smsg) {
				mutex_lock(&devdata->msg_queue_lock);
				list_del(&smsg->list);
				devdata->msg_queue_item_count--;
				mutex_unlock(&devdata->msg_queue_lock);

				if (prepared_smsg) {
					BUG_ON(smsg != prepared_smsg);
					devdata->spi_prepare_ops.unprepare_message(spi->master, &prepared_smsg->spi_msg);
					spi->master->cur_msg = NULL;
					prepared_smsg = NULL;
				}

				kfree(smsg);
			}
		} else {
			/* To avoid noise in the log, only log an error if we
			 * haven't recently reset the mcu
			 */
			if (!recent_reset_event(devdata))
				dev_warn_ratelimited(&spi->dev,
					"SPI transaction rejected by SyncBoss");
			devdata->stats.num_rejected_transactions++;
		}
	}

	if (prepared_smsg) {
		devdata->spi_prepare_ops.unprepare_message(spi->master, &prepared_smsg->spi_msg);
		spi->master->cur_msg = NULL;
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
	u8 message_buf[sizeof(struct syncboss_data) +
		       sizeof(struct prox_config_data)] = {};
	struct syncboss_data *message = (struct syncboss_data *)message_buf;
	struct prox_config_version *prox_cfg_version =
		(struct prox_config_version *)message->data;
	struct prox_config_data *prox_cal =
		(struct prox_config_data *)message->data;
	size_t data_len = 0;

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

		prox_cfg_version->type =
			SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE;
		prox_cfg_version->config_version = devdata->prox_config_version;

		data_len = sizeof(struct syncboss_data) + message->data_len;
		queue_tx_packet(devdata, message, data_len, /* from_user */ false);

		/* Set prox calibration */
		message->type = SYNCBOSS_SET_DATA_MESSAGE_TYPE;
		message->sequence_id = 0;
		message->data_len = sizeof(*prox_cal);

		prox_cal->type = SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE;
		prox_cal->prox_thdh = (u16)devdata->prox_thdh;
		prox_cal->prox_thdl = (u16)devdata->prox_thdl;
		prox_cal->prox_canc = (u16)devdata->prox_canc;

		data_len = sizeof(struct syncboss_data) + message->data_len;
		queue_tx_packet(devdata, message, data_len, /* from_user */ false);

	}

	/* Enable or disable prox */
	message->type = enable ? SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE :
		SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE;
	message->sequence_id = 0;
	message->data_len = 0;

	data_len = sizeof(struct syncboss_data) + message->data_len;
	queue_tx_packet(devdata, message, data_len, /* from_user */ false);
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

	dev_info(&devdata->spi->dev, "Waiting for syncboss to be %s", awake ? "awake" : "asleep");
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

static int start_streaming_locked(struct syncboss_dev_data *devdata,
				 bool force_reset)
{
	int status = 0;
	bool mcu_awake;

	if (devdata->is_streaming) {
		dev_warn(&devdata->spi->dev, "streaming already started");
		return 0;
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
		syncboss_pin_reset(devdata);
		status = wait_for_syncboss_wake_state(devdata, true);
		if (status)
			goto error;
		devdata->force_reset_on_open = false;
	} else {
		dev_info(&devdata->spi->dev,
			 "Skipping mcu reset since it's already awake");
	}

	dev_info(&devdata->spi->dev, "Starting stream");
	devdata->spi_prepare_ops.prepare_transfer_hardware(devdata->spi->master);
	devdata->transaction_ctr = 0;

	if (devdata->use_fastpath)
		devdata->spi_prepare_ops.prepare_message(devdata->spi->master, &devdata->default_smsg->spi_msg);

	/* The worker thread should not be running yet, so the worker
	 * should always be null here */
	BUG_ON(devdata->worker != NULL);

	devdata->worker = kthread_create(syncboss_spi_transfer_thread,
					 devdata->spi, "syncboss:spi_thread");
	if (IS_ERR(devdata->worker)) {
		status = PTR_ERR(devdata->worker);
		dev_err(&devdata->spi->dev, "Failed to start kernel thread. (%d)",
			status);
		devdata->worker = NULL;
		goto error;
	}

	dev_info(&devdata->spi->dev, "Setting SPI thread cpu affinity: %*pb",
		cpumask_pr_args(&devdata->cpu_affinity));
	kthread_bind_mask(devdata->worker, &devdata->cpu_affinity);

	devdata->is_streaming = true;
	wake_up_process(devdata->worker);

	push_prox_cal_and_enable_wake(devdata, devdata->prox_wake_enabled);

error:
	return status;
}

static void shutdown_syncboss_mcu_locked(struct syncboss_dev_data *devdata)
{
	int is_asleep = 0;
	struct syncboss_data message = {
		.type = SYNCBOSS_SHUTDOWN_MESSAGE_TYPE,
		.sequence_id = 0,
		.data_len = 0
	};

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));

	dev_info(&devdata->spi->dev, "Telling SyncBoss to go to sleep");

	/* Send command to put the MCU to sleep */
	queue_tx_packet(devdata, &message, sizeof(message), /* from_user */ false);

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

static void stop_streaming_locked(struct syncboss_dev_data *devdata)
{
	int status;
	struct syncboss_msg *smsg, *temp_smsg;

	if (!devdata->is_streaming) {
		dev_warn(&devdata->spi->dev, "streaming already stopped");
		return;
	}

	dev_info(&devdata->spi->dev, "Stopping stream");
	devdata->is_streaming = false;

	/* Tell syncboss to go to sleep */
	shutdown_syncboss_mcu_locked(devdata);

	if (devdata->cameras_enabled) {
		dev_warn(&devdata->spi->dev,
			"Cameras still enabled at shutdown. Manually forcing camera release");
		syncboss_on_camera_release(devdata);
	}

	kthread_stop(devdata->worker);
	devdata->worker = NULL;
	/* Now that device comm is totally down, signal the SYSTEM_DOWN
	 * event
	 */
	signal_prox_event_locked(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_DOWN);

	if (devdata->use_fastpath)
		devdata->spi_prepare_ops.unprepare_message(devdata->spi->master, &devdata->default_smsg->spi_msg);

	devdata->spi_prepare_ops.unprepare_transfer_hardware(devdata->spi->master);

	/* Discard and free and pending messages that were to be sent */
	mutex_lock(&devdata->msg_queue_lock);
	list_for_each_entry_safe(smsg, temp_smsg, &devdata->msg_queue_list, list) {
		list_del(&smsg->list);
		kfree(smsg);
	}
	mutex_unlock(&devdata->msg_queue_lock);

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

static void prox_wake_set_locked(struct syncboss_dev_data *devdata, bool enable)
{
	int status = 0;
	bool must_wake_syncboss = !devdata->is_streaming;

	if (must_wake_syncboss) {
		/* We're about to wake up syncboss, and then shut it
		 * down again.  If we don't silence the prox messages,
		 * the user will get a confusing stream of messages
		 * during this process.
		 */
		devdata->silence_all_prox_events = true;

		status = start_streaming_locked(devdata, true);
		if (status)
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
		stop_streaming_locked(devdata);

	devdata->silence_all_prox_events = false;
}

static void prox_wake_enable_locked(struct syncboss_dev_data *devdata)
{
	prox_wake_set_locked(devdata, true);
}

static void prox_wake_disable_locked(struct syncboss_dev_data *devdata)
{
	prox_wake_set_locked(devdata, false);
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
		signal_prox_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_UP);
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
	const u64 timestamp_ns = ktime_to_ns(ktime_get());
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
	devdata->boots_to_shutdown_state = of_property_read_bool(node, "oculus,syncboss-boots-to-shutdown-state");

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

	mutex_init(&devdata->state_mutex);

	return 0;
}

static int syncboss_probe(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata;
	struct device *dev = &spi->dev;
	struct spi_transfer *spi_xfer;
	struct syncboss_msg *default_smsg;

	int status = 0;

	dev_info(dev, "syncboss device initializing");
	dev_info(
		dev,
		"name: %s, max speed: %d, cs: %d, bits/word: %d, mode: 0x%x, irq: %d, modalias: %s, cs_gpio: %d",
		dev_name(dev), spi->max_speed_hz, spi->chip_select,
		spi->bits_per_word, spi->mode, spi->irq, spi->modalias,
		spi->cs_gpio);

	devdata = devm_kzalloc(dev, sizeof(*devdata), GFP_KERNEL);
	if (!devdata) {
		status = -ENOMEM;
		goto error;
	}

	devdata->swd_ops.is_busy = is_busy;
	devdata->swd_ops.fw_update_cb = fw_update_cb;

	status = init_syncboss_dev_data(devdata, spi);
	if (status < 0)
		goto error;

	/*
	 * Take ownership of calling prepare/unprepare away from the SPI
	 * framework, so can manage calls more efficiently for our usecase.
	 */
	if (devdata->use_fastpath) {
		devdata->spi_prepare_ops.prepare_message = spi->master->prepare_message;
		devdata->spi_prepare_ops.unprepare_message = spi->master->unprepare_message;
		spi->master->prepare_message = NULL;
		spi->master->unprepare_message = NULL;
	}
	devdata->spi_prepare_ops.prepare_transfer_hardware = spi->master->prepare_transfer_hardware;
	devdata->spi_prepare_ops.unprepare_transfer_hardware = spi->master->unprepare_transfer_hardware;
	spi->master->prepare_transfer_hardware = NULL;
	spi->master->unprepare_transfer_hardware = NULL;

	devdata->rx_elem = devm_kzalloc(dev, sizeof(*devdata->rx_elem),
					GFP_KERNEL | GFP_DMA);
	if (!devdata->rx_elem) {
		status = -ENOMEM;
		goto error_after_devdata_init;
	}

	default_smsg = devm_kzalloc(dev, sizeof(*devdata->default_smsg),
				    GFP_KERNEL | GFP_DMA);
	if (!default_smsg) {
		status = -ENOMEM;
		goto error_after_devdata_init;
	}
	devdata->default_smsg = default_smsg;

	default_smsg->tx.header.magic_num = SPI_DATA_MAGIC_NUM;
	default_smsg->tx.header.checksum = calculate_checksum(&default_smsg->tx,
				     devdata->transaction_length);

	spi_xfer = &default_smsg->spi_xfer;
	spi_xfer->tx_buf = &default_smsg->tx;
	spi_xfer->rx_buf = devdata->rx_elem->buf;
	spi_xfer->len = devdata->transaction_length;
	spi_xfer->bits_per_word = 8;
	spi_message_init(&default_smsg->spi_msg);
	spi_message_add_tail(spi_xfer, &default_smsg->spi_msg);
	default_smsg->spi_msg.spi = spi;

	mutex_init(&devdata->msg_queue_lock);
	INIT_LIST_HEAD(&devdata->msg_queue_list);

	fw_init_regulator(dev, &devdata->hall_sensor, "hall-sensor");
	fw_init_regulator(dev, &devdata->mcu_core, "mcu-core");
	fw_init_regulator(dev, &devdata->imu_core, "imu-core");
	fw_init_regulator(dev, &devdata->mag_core, "mag-core");
	fw_init_regulator(dev, &devdata->rf_amp, "rf-amp");

	dev_set_drvdata(dev, devdata);

	syncboss_init_pins(dev);

	/* misc dev stuff */
	devdata->misc.name = SYNCBOSS_DEVICE_NAME;
	devdata->misc.minor = MISC_DYNAMIC_MINOR;
	devdata->misc.fops = &fops;

	status = misc_register(&devdata->misc);
	if (status < 0) {
		dev_err(dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_devdata_init;
	}

	devdata->stream_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->stream_fifo.config.header_payload = true;
	devdata->stream_fifo.config.filter_fn = should_send_stream_packet;
	status = miscfifo_register(&devdata->stream_fifo);
	if (status < 0) {
		dev_err(dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_misc_register;
	}

	devdata->misc_stream.name = SYNCBOSS_STREAM_DEVICE_NAME;
	devdata->misc_stream.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_stream.fops = &stream_fops;

	status = misc_register(&devdata->misc_stream);
	if (status < 0) {
		dev_err(dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_stream_fifo_register;
	}

	devdata->control_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->control_fifo.config.header_payload = true;
	status = miscfifo_register(&devdata->control_fifo);
	if (status < 0) {
		dev_err(dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_stream_register;
	}

	devdata->misc_control.name = SYNCBOSS_CONTROL_DEVICE_NAME;
	devdata->misc_control.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_control.fops = &control_fops;

	status = misc_register(&devdata->misc_control);
	if (status < 0) {
		dev_err(dev, "%s fails to register misc device, error %d",
			__func__, status);
		goto error_after_control_fifo_register;
	}

	devdata->prox_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->prox_fifo.config.header_payload = true;
	status = miscfifo_register(&devdata->prox_fifo);
	if (status < 0) {
		dev_err(dev,
			"%s fails to register miscfifo device, error %d",
			__func__, status);
		goto error_after_control_register;
	}

	devdata->misc_prox.name = SYNCBOSS_PROX_DEVICE_NAME;
	devdata->misc_prox.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_prox.fops = &prox_fops;

	status = misc_register(&devdata->misc_prox);
	if (status < 0) {
		dev_err(dev, "%s fails to register misc device, error %d",
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
			dev_err(dev, "Failed to register misc nsync device, error %d",
					status);
		}
	}

	/*
	 * Create child SWD fw-update devices, if any
	 */
	status = of_platform_populate(dev->of_node, oculus_swd_match_table,
				      NULL, dev);
	if (status < 0)
		goto error_after_prox_register;

	status = syncboss_init_sysfs_attrs(devdata);
	if (status < 0)
		goto error_after_of_platform_populate;

	devdata->power_state = SYNCBOSS_POWER_STATE_RUNNING;
	devdata->enable_headers = true;

	/* Init interrupts */
	if (devdata->gpio_wakeup < 0) {
		dev_err(dev, "Missing 'oculus,syncboss-wakeup' GPIO");
		status = devdata->gpio_wakeup;
		goto error_after_sysfs;
	}
	devdata->wakeup_irq = gpio_to_irq(devdata->gpio_wakeup);
	irq_set_status_flags(devdata->wakeup_irq, IRQ_DISABLE_UNLAZY);
	/* This irq must be able to wake up the system */
	irq_set_irq_wake(devdata->wakeup_irq, /*on*/ 1);
	status =
		devm_request_threaded_irq(dev, devdata->wakeup_irq,
					  isr_primary_wakeup, isr_thread_wakeup,
					  IRQF_ONESHOT | IRQF_TRIGGER_RISING,
					  devdata->misc.name, devdata);
	if (status < 0) {
		dev_err(dev,
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
			dev_err(dev,
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
	device_init_wakeup(dev, /*enable*/ true);

	wakeup_source_init(&devdata->syncboss_in_use_wake_lock, "syncboss");

	if (!devdata->boots_to_shutdown_state) {
		/*
		 * This looks kinda hacky, but starting and then stopping the stream is
		 * the simplest way to get the SyncBoss in an initial sleep state.
		 */
		reset_syncboss(devdata);
	}
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
error_after_devdata_init:
	if (devdata->use_fastpath) {
		spi->master->prepare_message = devdata->spi_prepare_ops.prepare_message;
		spi->master->unprepare_message = devdata->spi_prepare_ops.unprepare_message;
	}
	spi->master->prepare_transfer_hardware = devdata->spi_prepare_ops.prepare_transfer_hardware;
	spi->master->unprepare_transfer_hardware = devdata->spi_prepare_ops.unprepare_transfer_hardware;
	flush_workqueue(devdata->syncboss_workqueue);
	destroy_workqueue(devdata->syncboss_workqueue);
error:
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

	flush_workqueue(devdata->syncboss_workqueue);
	destroy_workqueue(devdata->syncboss_workqueue);

	/* Return prepare/unprepare call ownership to the kernel SPI framework. */
	if (devdata->use_fastpath) {
		spi->master->prepare_message = devdata->spi_prepare_ops.prepare_message;
		spi->master->unprepare_message = devdata->spi_prepare_ops.unprepare_message;
	}
	spi->master->prepare_transfer_hardware = devdata->spi_prepare_ops.prepare_transfer_hardware;
	spi->master->unprepare_transfer_hardware = devdata->spi_prepare_ops.unprepare_transfer_hardware;

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

static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct syncboss_dev_data *devdata =
		container_of(filp->private_data, struct syncboss_dev_data,
			     misc);

	return queue_tx_packet(devdata, buf, count, /* from_user */ true);
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
