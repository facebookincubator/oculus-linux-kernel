// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/poll.h>
#include <linux/pm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/wait.h>
#include <linux/regulator/consumer.h>

#include "../fw_helpers.h"

#include "spi_fastpath.h"
#include "syncboss.h"
#include "syncboss_debugfs.h"
#include "syncboss_sequence_number.h"
#include "syncboss_protocol.h"
#include "syncboss_direct_channel.h"

#ifdef CONFIG_OF /*Open firmware must be defined for dts usage*/
static const struct of_device_id syncboss_spi_table[] = {
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

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 3, 0)
#define ktime_get_boottime_ns ktime_get_boot_ns
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

#define SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS 0
#define SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS 0
#define SYNCBOSS_DEFAULT_MAX_MSG_SEND_DELAY_NS 0
#define SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE 8000000
#define SYNCBOSS_SCHEDULING_SLOP_NS 10000
/*
 * The max amount of time we give SyncBoss to wake or enter its sleep state.
 */
#define SYNCBOSS_SLEEP_WAKE_TIMEOUT_MS 2000
#define SYNCBOSS_SLEEP_WAKE_TIMEOUT_WITH_GRACE_MS 5000

/* The amount of time we should hold the reset line low when doing a reset. */
#define SYNCBOSS_RESET_TIME_MS 5

#define SPI_TX_DATA_MAGIC_NUM 0xDEFEC8ED
#define SPI_RX_DATA_MAGIC_NUM_POLLING_MODE 0xDEFEC8ED
#define SPI_RX_DATA_MAGIC_NUM_IRQ_MODE 0xD00D1E8D
#define SPI_RX_DATA_MAGIC_NUM_MCU_BUSY 0xCACACACA

#define SYNCBOSS_MISCFIFO_SIZE 1024

#define SYNCBOSS_NSYNC_MISCFIFO_SIZE 256

#define SYNCBOSS_DEFAULT_POLL_PRIO 51

/*
 * These are SPI message types that the driver explicitly monitors and
 * sends to SyncBoss firmware. They must be kept in sync.
 */
#define SYNCBOSS_GET_DATA_MESSAGE_TYPE 2
#define SYNCBOSS_SET_DATA_MESSAGE_TYPE 3
#define SYNCBOSS_CAMERA_PROBE_MESSAGE_TYPE 40
#define SYNCBOSS_CAMERA_RELEASE_MESSAGE_TYPE 41
#define SYNCBOSS_SHUTDOWN_MESSAGE_TYPE 90
#define SYNCBOSS_PROX_ENABLE_MESSAGE_TYPE 203
#define SYNCBOSS_PROX_DISABLE_MESSAGE_TYPE 204
#define SYNCBOSS_SET_PROX_CAL_MESSAGE_TYPE 205
#define SYNCBOSS_PROXSTATE_MESSAGE_TYPE 207
#define SYNCBOSS_SET_PROX_CONFIG_VERSION_MESSAGE_TYPE 212
#define SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE 244

#define INVALID_PROX_CAL_VALUE -1
#define INVALID_POWERSTATE_VALUE -1

#define DEFAULT_PROX_CONFIG_VERSION_VALUE 1

/*
 * The amount of time to hold the wake-lock when syncboss wakes up the
 * AP.  This allows user-space code to have time to react before the
 * system automatically goes back to sleep.
 */
#define SYNCBOSS_WAKEUP_EVENT_DURATION_MS 2000

/*
 * The amount of time to suppress spurious SPI errors after a syncboss
 * reset
 */
#define SYNCBOSS_RESET_SPI_SETTLING_TIME_MS 100

/*
 * Maximum pending messages enqueued by syncboss_write()
 */
#define MAX_MSG_QUEUE_ITEMS 100

/* The version of the header the driver is currently using */
#define SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION SYNCBOSS_DRIVER_HEADER_VERSION_V1

struct transaction_context {
	bool msg_to_send;
	bool reschedule_needed;
	struct syncboss_msg *smsg;
	struct syncboss_msg *prepared_smsg;
	bool data_ready_irq_had_fired;
	bool send_timer_had_fired;
	bool wake_timer_had_fired;
	bool control_fifo_wake_needed;
	bool stream_fifo_wake_needed;
};

static void syncboss_on_camera_probe(struct syncboss_dev_data *devdata);
static void syncboss_on_camera_release(struct syncboss_dev_data *devdata);
static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos);
static int wake_mcu(struct syncboss_dev_data *devdata, bool force_pin_reset);
static int wait_for_syncboss_wake_state(struct syncboss_dev_data *devdata,
					bool state);

/*
 * This is a function that miscfifo calls to determine if it should
 * send a given packet to a given client.
 */
static bool should_send_stream_packet(const void *context,
				      const u8 *header, size_t header_len,
				      const u8 *payload, size_t payload_len)
{
	int x = 0;
	const struct syncboss_driver_stream_type_filter *stream_type_filter =
		context;
	const struct uapi_pkt_t *uapi_pkt = (struct uapi_pkt_t *) payload;
	const struct syncboss_data *packet;

	/* Special case for when no filter is set or there's no payload */
	if (!stream_type_filter || (stream_type_filter->num_selected == 0) ||
	    !payload)
		return true;

	packet = (struct syncboss_data *) uapi_pkt->payload;
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

	status = firmware_request_nowarn(&fw, cal_file_name, &devdata->spi->dev);
	if (status != 0) {
		dev_err(&devdata->spi->dev,
			"firmware_request_nowarn() returned %d. Ensure %s is present",
			status, cal_file_name);
		return status;
	}

	if (fw->size >= sizeof(tempstr)) {
		dev_err(&devdata->spi->dev,
			"unexpected size for %s (size is %zd)",
			cal_file_name, fw->size);
		status = -EINVAL;
		goto error;
	}

	/* Copy to temp buffer to ensure null-termination */
	memcpy(tempstr, fw->data, fw->size);
	tempstr[fw->size] = '\0';

	status = kstrtou32(tempstr, /*base */10, &temp_parse);
	if (status < 0) {
		dev_err(&devdata->spi->dev, "failed to parse integer out of %s",
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
	/*
	 * Note: There are cases in the factory where they need to
	 * just set prox_canc without setting the other values.  Given
	 * this use-case, we consider a prox cal "valid" if anything
	 * is >= zero.
	 */
	return (devdata->prox_canc >= 0) || (devdata->prox_thdl >= 0)
		|| (devdata->prox_thdh >= 0);
}

static void read_prox_cal(struct syncboss_dev_data *devdata)
{
	/* If prox doesn't require calibration, use dummy values. */
	if (devdata->has_no_prox_cal) {
		devdata->prox_config_version = 0;
		devdata->prox_canc = 0;
		devdata->prox_thdl = 0;
		return;
	}

	devdata->prox_config_version = read_cal_int(devdata,
						    "PROX_PS_CAL_VERSION");
	devdata->prox_canc = read_cal_int(devdata, "PROX_PS_CANC");
	devdata->prox_thdl = read_cal_int(devdata, "PROX_PS_THDL");
	devdata->prox_thdh = read_cal_int(devdata, "PROX_PS_THDH");

	if (!prox_cal_valid(devdata)) {
		dev_err(&devdata->spi->dev,
			"failed read prox calibration data (ver: %u, canc: %d, thdl: %d, thdh: %d)",
			devdata->prox_config_version, devdata->prox_canc,
			devdata->prox_thdl, devdata->prox_thdh);
		return;
	}

	dev_info(&devdata->spi->dev,
		 "prox cal read: canc: %d, thdl: %d, thdh: %d",
		 devdata->prox_canc, devdata->prox_thdl, devdata->prox_thdh);
}

static void syncboss_inc_mcu_client_count_locked(struct syncboss_dev_data *devdata)
{
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->mcu_client_count < 0);

	if (devdata->mcu_client_count++ == 0) {
		/*
		 * Wake the MCU by pin reset.
		 *
		 * Ignoring the return value here because wait_for_syncboss_wake_state logs
		 * its own errors. Also, current callers eventually double-check the state.
		 *
		 * We double-check the state already after all current calls to this
		 * function prior to requiring a particular state. If we add cases where
		 * this is not true or if there is long-running work that can be done in
		 * parallel with waiting for syncboss to be awake, we may want to propagate
		 * a reset_released signal to callers so they can choose when to execute
		 * the wait.
		 */
		wake_mcu(devdata, true);
	}
}

static void syncboss_dec_mcu_client_count_locked(struct syncboss_dev_data *devdata)
{
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->mcu_client_count < 1);

	if (--devdata->mcu_client_count == 0) {
		dev_info(&devdata->spi->dev, "asserting MCU reset");
		gpio_set_value(devdata->gpio_reset, 0);
	}
}

static int start_streaming_locked(struct syncboss_dev_data *devdata);
static void stop_streaming_locked(struct syncboss_dev_data *devdata);

static bool is_busy(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	return devdata->is_streaming;
}

static int syncboss_inc_streaming_client_count_locked(struct syncboss_dev_data *devdata)
{
	int status = 0;

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->streaming_client_count < 0);

	/*
	 * The first time a client opens a handle to the driver, read
	 * the prox sensor calibration.  We do it here as opposed to
	 * in the SPI transfer thread because request_firmware must be
	 * called in a user-context.  We read the calibration every
	 * time to support the factory case where prox calibration is
	 * being modified and we don't want to force a headset reboot
	 * for the new calibration to take effect.
	 */
	if (devdata->has_prox)
		read_prox_cal(devdata);

	if (devdata->streaming_client_count == 0) {
		syncboss_inc_mcu_client_count_locked(devdata);

		dev_dbg(&devdata->spi->dev, "starting streaming thread");
		status = start_streaming_locked(devdata);
		if (status)
			syncboss_dec_mcu_client_count_locked(devdata);
	}

	if (!status)
		++devdata->streaming_client_count;

	return status;
}

static int syncboss_dec_streaming_client_count_locked(struct syncboss_dev_data *devdata)
{
	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->streaming_client_count < 1);

	--devdata->streaming_client_count;
	if (devdata->streaming_client_count == 0) {
		dev_dbg(&devdata->spi->dev, "stopping streaming thread");
		stop_streaming_locked(devdata);

		syncboss_dec_mcu_client_count_locked(devdata);

		/*
		 * T114417103 Resetting the ioctl interface state shouldn't be necessary
		 * but it doesn't hurt and may help if there is a case where we are
		 * leaking for some reason.
		 * Reset the mode to allow changing modes by stopping all clients.
		 */
		syncboss_sequence_number_reset_locked(devdata);
	}
	return 0;
}

static int fw_update_check_busy(struct device *dev, void *data)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "oculus,swd")) {
		struct swd_dev_data *devdata = dev_get_drvdata(dev);

		if (!devdata) {
			dev_err(dev, "swd device not yet available: %s", node->name);
			return -ENODEV;
		}

		if (devdata->fw_update_state != FW_UPDATE_STATE_IDLE) {
			return -EBUSY;
		}
	}
	return 0;
}

static void client_data_init_locked(struct syncboss_dev_data *devdata)
{
	INIT_LIST_HEAD(&devdata->client_data_list);
	devdata->client_data_index = 0;
}

static int client_data_create_locked(struct syncboss_dev_data *devdata,
		struct file *file, struct task_struct *task,
		struct syncboss_client_data **client_data)
{
	struct device *dev = &devdata->spi->dev;
		struct syncboss_client_data *data;
	int status;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	INIT_LIST_HEAD(&data->list_entry);
	data->file = file;
	data->task = task;
	data->index = devdata->client_data_index++;
	data->seq_num_allocation_count = 0;
	bitmap_zero(data->allocated_seq_num, SYNCBOSS_SEQ_NUM_BITS);

	data->index = devdata->client_data_index++;
	status = syncboss_debugfs_client_add_locked(devdata, data);
	if (status && -ENODEV != status)
		dev_warn(dev, "failed to add client data to debugfs for %d: %d",
			data->task->pid, status);

	list_add_tail(&data->list_entry, &devdata->client_data_list);

	*client_data = data;

	return 0;
}

static struct syncboss_client_data *client_data_get_locked(
	struct syncboss_dev_data *devdata, struct file *file)
{
	struct syncboss_client_data *data;

	list_for_each_entry(data, &devdata->client_data_list, list_entry) {
		if (data->file == file)
			return data;
	}

	return NULL;
}

static void client_data_destroy_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
{
	struct device *dev = &devdata->spi->dev;

	syncboss_debugfs_client_remove_locked(devdata, client_data);

	list_del(&client_data->list_entry);

	syncboss_sequence_number_release_client_locked(devdata, client_data);

	devm_kfree(dev, client_data);
}

static int syncboss_open(struct inode *inode, struct file *f)
{
	int status;
	struct syncboss_dev_data *devdata =
	    container_of(f->private_data, struct syncboss_dev_data, misc);
	struct device *dev = &devdata->spi->dev;
	struct syncboss_client_data *client_data;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status) {
		dev_warn(&devdata->spi->dev, "syncboss open by %s (%d) aborted due to signal. status=%d",
			current->comm, current->pid, status);
		goto ret;
	}

	status = device_for_each_child(&devdata->spi->dev, NULL,
				       fw_update_check_busy);
	if (status) {
		if (status == -EBUSY)
			dev_err(dev, "syncboss opened by %s (%d) failed, firmware update in progress",
				current->comm, current->pid);
		goto unlock;
	}

	status = syncboss_inc_streaming_client_count_locked(devdata);
	if (status) {
		dev_err(dev, "syncboss open by %s (%d) failed to increment streaming client count (%d)",
			current->comm, current->pid, status);
		goto unlock;
	}

	status = client_data_create_locked(devdata, f, current, &client_data);
	if (status) {
		dev_err(dev, "syncboss open by %s (%d) failed to create client data (%d)",
			current->comm, current->pid, status);
		goto dec_streaming_client;
	}

	dev_info(dev, "syncboss opened by %s (%d), client %lld",
		client_data->task->comm, client_data->task->pid, client_data->index);

	goto unlock;

dec_streaming_client:
	syncboss_dec_streaming_client_count_locked(devdata);

unlock:
	mutex_unlock(&devdata->state_mutex);

ret:
	return status;
}

static long syncboss_ioctl_handle_seq_num_allocate_locked(
	struct syncboss_dev_data *devdata, struct syncboss_client_data *client_data,
	unsigned long arg)
{
	struct device *dev = &devdata->spi->dev;
	long status;
	long status_release;
	uint8_t seq;

	if (!devdata->has_seq_num_ioctl) {
		dev_err_ratelimited(dev,
			"sequence number ioctls are disabled based on device tree configuration. requested by %s (%d), client %lld",
			client_data->task->comm, client_data->task->pid, client_data->index);
		status = -EPERM;
		goto ret;
	}

	status = syncboss_sequence_number_allocate_locked(devdata, client_data, &seq);
	if (status) {
		dev_err(dev,
			"failed to allocate sequence number: %ld. requested by %s (%d), client %lld",
			status, client_data->task->comm, client_data->task->pid,
			client_data->index);
		goto ret;
	}

	status = copy_to_user((uint8_t __user *)arg, &seq, sizeof(seq));
	if (status) {
		dev_err(dev,
			"failed to copy sequence number %d to user: %ld. requested by %s (%d), client %lld",
			seq, status, client_data->task->comm, client_data->task->pid,
			client_data->index);
		status = -EFAULT;
		goto seq_num_release;
	}

	goto ret;

seq_num_release:
	/* Don't clobber the original error */
	status_release = syncboss_sequence_number_release_locked(devdata, client_data, seq);
	if (status_release) {
		dev_err(dev,
			"leaking sequence number %d due to failure to release on error: %ld. requested by %s (%d), client %lld",
			seq, status_release, client_data->task->comm, client_data->task->pid,
			client_data->index);
	}

ret:
	return status;
}

static long syncboss_ioctl_handle_seq_num_release_locked(
	struct syncboss_dev_data *devdata, struct syncboss_client_data *client_data,
	unsigned long arg)
{
	struct device *dev = &devdata->spi->dev;
	long status;
	uint8_t seq;

	if (!devdata->has_seq_num_ioctl) {
		dev_err_ratelimited(dev,
			"sequence number iocls are disabled based on device tree configuration. requested by %s (%d), client %lld",
			client_data->task->comm, client_data->task->pid, client_data->index);
		return -EPERM;
	}

	status = copy_from_user(&seq, (uint8_t __user *)arg, sizeof(seq));
	if (status) {
		dev_err(dev,
			"failed to copy sequence number from user: %ld. requested by %s (%d), client %lld",
			status, client_data->task->comm, client_data->task->pid, client_data->index);
		return -EFAULT;
	}

	status = syncboss_sequence_number_release_locked(devdata, client_data, seq);
	if (status) {
		dev_err(dev,
			"leaking sequence number %d due to failure to release: %ld. requested by %s (%d), client %lld",
			seq, status, client_data->task->comm, client_data->task->pid,
			client_data->index);
		return status;
	}

	return status;
}

static long syncboss_ioctl(struct file *file, unsigned int cmd,
				  unsigned long arg)
{
	int status = 0;
	struct syncboss_client_data *client_data;
	struct syncboss_dev_data *devdata =
		container_of(file->private_data, struct syncboss_dev_data, misc);
	struct device *dev = &devdata->spi->dev;

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status != 0) {
		dev_warn(&devdata->spi->dev, "ioctl from %s (%d) aborted due to signal. status=%d",
			 current->comm, current->pid, status);
		goto ret;
	}

	client_data = client_data_get_locked(devdata, file);
	if (!client_data) {
		dev_err(dev, "ioctl from %s (%d) failed to get client data)",
			current->comm, current->pid);
		status = -ENXIO;
		goto unlock;
	}

	switch (cmd) {
	case SYNCBOSS_SEQUENCE_NUMBER_ALLOCATE_IOCTL:
		status = syncboss_ioctl_handle_seq_num_allocate_locked(devdata,
					client_data, arg);
		break;
	case SYNCBOSS_SEQUENCE_NUMBER_RELEASE_IOCTL:
		status = syncboss_ioctl_handle_seq_num_release_locked(devdata,
					client_data, arg);
		break;
	default:
		dev_err(dev, "unrecognized ioctl %d, from %s (%d), client %lld",
			cmd, current->comm, current->pid, client_data->index);
		status = -EINVAL;
		break;
	}

unlock:
	mutex_unlock(&devdata->state_mutex);

ret:
	return status;
}

static int signal_powerstate_event(struct syncboss_dev_data *devdata, int evt)
{
	int status = 0;
	bool should_update_last_evt = true;

	struct syncboss_driver_data_header_driver_message_t msg = {
		.header = {
			.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
			.header_length = sizeof(struct syncboss_driver_data_header_driver_message_t),
			.from_driver = true,
		},
		.driver_message_type = SYNCBOSS_DRIVER_MESSAGE_POWERSTATE_MSG,
		.driver_message_data = evt,
	};

	if ((evt == SYNCBOSS_PROX_EVENT_SYSTEM_UP) &&
	    devdata->eat_next_system_up_event) {
		/* This is a manual reset, so no need to notify clients */
		dev_info(&devdata->spi->dev, "eating prox system_up event on reset..yum!");
		devdata->eat_next_system_up_event = false;
		/* We don't want anyone who opens a powerstate handle to see this event. */
		should_update_last_evt = false;
	} else if (evt == SYNCBOSS_PROX_EVENT_PROX_ON && devdata->eat_prox_on_events) {
		dev_info(&devdata->spi->dev, "sensor still covered. eating prox_on event..yum!");
	} else if (devdata->silence_all_powerstate_events) {
		dev_info(&devdata->spi->dev, "silencing powerstate event %d", evt);
		/* We don't want anyone who opens a powerstate handle to see this event. */
		should_update_last_evt = false;
	} else {
		if (evt == SYNCBOSS_PROX_EVENT_PROX_OFF && devdata->eat_prox_on_events) {
			dev_info(&devdata->spi->dev, "prox_off received. Resuming handling of prox_on events.");
			devdata->eat_prox_on_events = false;
		}

		status = miscfifo_send_buf(&devdata->powerstate_fifo, (u8 *) &msg, sizeof(msg));
		if (status < 0)
			dev_warn_ratelimited(&devdata->spi->dev, "powerstate fifo error (%d)", status);
	}

	if (should_update_last_evt)
		devdata->powerstate_last_evt = evt;

	return status;
}

static void powerstate_enable_locked(struct syncboss_dev_data *devdata);
static void powerstate_disable_locked(struct syncboss_dev_data *devdata);

static int syncboss_powerstate_open(struct inode *inode, struct file *f)
{
	int status;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_powerstate);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status) {
		dev_warn(&devdata->spi->dev, "syncboss open by %s (%d) aborted due to signal. status=%d",
				current->comm, current->pid, status);
		return status;
	}

	status = miscfifo_fop_open(f, &devdata->powerstate_fifo);
	if (status)
		goto out;

	++devdata->powerstate_client_count;

	/* Send the last powerstate_event */
	if (devdata->powerstate_last_evt != INVALID_POWERSTATE_VALUE) {
		dev_info(&devdata->spi->dev,
			 "signaling powerstate_last_evt (%d)",
			 devdata->powerstate_last_evt);
		signal_powerstate_event(devdata, devdata->powerstate_last_evt);
	}

	if (devdata->powerstate_client_count == 1) {
		syncboss_inc_mcu_client_count_locked(devdata);

		if (devdata->has_prox)
			read_prox_cal(devdata);

		dev_dbg(&devdata->spi->dev, "enabling powerstate events");
		powerstate_enable_locked(devdata);
	}

	dev_info(&devdata->spi->dev, "powerstate opened by %s (%d)",
		 current->comm, current->pid);

out:
	mutex_unlock(&devdata->state_mutex);
	return status;
}

static int syncboss_powerstate_release(struct inode *inode, struct file *file)
{
	struct miscfifo_client *client = file->private_data;
	struct miscfifo *mf = client->mf;
	struct syncboss_dev_data *devdata =
		container_of(mf, struct syncboss_dev_data,
			     powerstate_fifo);

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->state_mutex);

	--devdata->powerstate_client_count;

	if (devdata->powerstate_client_count == 0) {
		dev_dbg(&devdata->spi->dev, "disabling powerstate events");
		powerstate_disable_locked(devdata);
		syncboss_dec_mcu_client_count_locked(devdata);
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
	dev_info(&devdata->spi->dev, "stream fifo opened by %s (%d)",
		 current->comm, current->pid);

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
	struct syncboss_driver_stream_type_filter *existing_filter;

	new_filter = devm_kzalloc(dev, sizeof(*new_filter), GFP_KERNEL);
	if (!new_filter)
		return -ENOMEM;

	status = copy_from_user(new_filter, filter, sizeof(*new_filter));
	if (status != 0) {
		dev_err(dev, "failed to copy %d bytes from user stream filter\n",
			status);
		return -EFAULT;
	}

	/* Sanity check new_filter */
	if (new_filter->num_selected > SYNCBOSS_MAX_FILTERED_TYPES) {
		dev_err(dev, "sanity check of user stream filter failed (num_selected = %d)\n",
			new_filter->num_selected);
		return -EINVAL;
	}

	existing_filter = miscfifo_fop_xchg_context(file, new_filter);
	if (existing_filter)
		devm_kfree(dev, existing_filter);
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
		dev_err(&devdata->spi->dev, "unrecognized stream ioctl %d from %s (%d)",
				cmd, current->comm, current->pid);
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

	stream_type_filter = miscfifo_fop_xchg_context(file, NULL);
	if (stream_type_filter)
		devm_kfree(&devdata->spi->dev, stream_type_filter);

	status = miscfifo_fop_release(inode, file);
	return status;
}

static int syncboss_control_open(struct inode *inode, struct file *f)
{
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_control);
	dev_info(&devdata->spi->dev, "control opened %s (%d)",
		 current->comm, current->pid);
	return miscfifo_fop_open(f, &devdata->control_fifo);
}

static s64 ktime_get_ms(void)
{
	return ktime_to_ms(ktime_get());
}

void syncboss_pin_reset(struct syncboss_dev_data *devdata)
{
	if (!gpio_is_valid(devdata->gpio_reset)) {
		dev_err(&devdata->spi->dev,
			"cannot pin-reset becauses reset pin was not specified in device tree");
		return;
	}

	dev_info(&devdata->spi->dev, "pin-resetting MCU");

	if (gpio_get_value(devdata->gpio_reset) == 1) {
		gpio_set_value(devdata->gpio_reset, 0);
		msleep(SYNCBOSS_RESET_TIME_MS);
	}

	/*
	 * Since we're triggering a reset, no need to notify clients
	 * when syncboss comes back up.
	 */
	devdata->eat_next_system_up_event = true;
	devdata->last_reset_time_ms = ktime_get_ms();
	gpio_set_value(devdata->gpio_reset, 1);
}

static int syncboss_release(struct inode *inode, struct file *f)
{
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data, misc);
	struct device *dev = &devdata->spi->dev;
	struct syncboss_client_data *client_data;

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->state_mutex);

	client_data = client_data_get_locked(devdata, f);
	BUG_ON(!client_data);

	dev_info(dev, "syncboss closed by %s (%d), client %lld",
		client_data->task->comm, client_data->task->pid, client_data->index);

	/* This must be done before syncboss_dec_streaming_client_count_locked
	 * since it will syncboss_sequence_number_reset_locked if the ref count
	 * goes to zero and reset checks for any remaining clients. */
	client_data_destroy_locked(devdata, client_data);

	syncboss_dec_streaming_client_count_locked(devdata);

	mutex_unlock(&devdata->state_mutex);

	return 0;
}

static irqreturn_t isr_primary_nsync(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;
	unsigned long flags;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	devdata->nsync_irq_timestamp = ktime_get_ns();
	++devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	return IRQ_WAKE_THREAD;
}

static irqreturn_t isr_thread_nsync(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;
	struct syncboss_nsync_event event;
	unsigned long flags;
	int status;

	spin_lock_irqsave(&devdata->nsync_lock, flags);
	event.timestamp = devdata->nsync_irq_timestamp;
	event.count = devdata->nsync_irq_count;
	spin_unlock_irqrestore(&devdata->nsync_lock, flags);

	status = miscfifo_send_buf(
			&devdata->nsync_fifo,
			(void *)&event,
			sizeof(event));
	if (status < 0)
		dev_warn_ratelimited(
			&devdata->spi->dev,
			"nsync fifo send failure: %d\n", status);

	return IRQ_HANDLED;
}

static int syncboss_nsync_open(struct inode *inode, struct file *f)
{
	int status;
	struct syncboss_dev_data *devdata =
		container_of(f->private_data, struct syncboss_dev_data,
			     misc_nsync);

	status = mutex_lock_interruptible(&devdata->state_mutex);
	if (status) {
		dev_warn(&devdata->spi->dev, "nsync open by %s (%d) borted due to signal. status=%d",
			       current->comm, current->pid, status);
		return status;
	}

	if (devdata->nsync_client_count != 0) {
		dev_err(&devdata->spi->dev, "nsync open by %s (%d) failed: nsync cannot be opened by more than one user",
				current->comm, current->pid);
		status = -EBUSY;
		goto unlock;
	}

	++devdata->nsync_client_count;

	status = miscfifo_fop_open(f, &devdata->nsync_fifo);
	if (status < 0) {
		dev_err(&devdata->spi->dev, "nsync miscfifo open by %s (%d) failed (%d)",
			current->comm, current->pid, status);
		goto decrement_ref;
	}

	devdata->nsync_irq_timestamp = 0;
	devdata->nsync_irq_count = 0;
	devdata->nsync_irq = gpio_to_irq(devdata->gpio_nsync);
	irq_set_status_flags(devdata->nsync_irq, IRQ_DISABLE_UNLAZY);
	status = devm_request_threaded_irq(
		&devdata->spi->dev, devdata->nsync_irq, isr_primary_nsync,
		isr_thread_nsync, IRQF_TRIGGER_RISING,
		devdata->misc_nsync.name, devdata);

	if (status < 0) {
		dev_err(&devdata->spi->dev, "nsync irq request by %s (%d) failed (%d)",
			current->comm, current->pid, status);
		goto miscfifo_release;
	}

	dev_info(&devdata->spi->dev, "nsync handle opened by %s (%d)",
		 current->comm, current->pid);

miscfifo_release:
	if (status < 0)
		miscfifo_fop_release(inode, f);
decrement_ref:
	if (status < 0)
		--devdata->nsync_client_count;
unlock:
	mutex_unlock(&devdata->state_mutex);
	return status;
}

static int syncboss_nsync_release(struct inode *inode, struct file *file)
{
	int status;
	struct miscfifo_client *client = file->private_data;
	struct syncboss_dev_data *devdata =
		container_of(client->mf, struct syncboss_dev_data, nsync_fifo);

	/*
	 * It is unsafe to use the interruptible variant here, as the driver can get
	 * in an inconsistent state if this lock fails. We need to make sure release
	 * handling occurs, since we can't retry releasing the file if, e.g., the
	 * file is released when a process is killed.
	 */
	mutex_lock(&devdata->state_mutex);

	--devdata->nsync_client_count;
	if (devdata->nsync_client_count == 0)
		devm_free_irq(&devdata->spi->dev, devdata->nsync_irq, devdata);

	miscfifo_clear(&devdata->nsync_fifo);

	status = miscfifo_fop_release(inode, file);
	if (status) {
		dev_err(&devdata->spi->dev, "nsync release by %s (%d) failed with status %d",
				current->comm, current->pid, status);
		goto out;
	}

	dev_info(&devdata->spi->dev, "nsync released by %s (%d)",
		 current->comm, current->pid);

out:
	mutex_unlock(&devdata->state_mutex);
	return status;
}

static const struct file_operations fops = {
	.open = syncboss_open,
	.release = syncboss_release,
	.read = NULL,
	.write = syncboss_write,
	.poll = NULL,
	.unlocked_ioctl = syncboss_ioctl
};

static const struct file_operations stream_fops = {
	.open = syncboss_stream_open,
	.release = syncboss_stream_release,
	.read = miscfifo_fop_read_many,
	.poll = miscfifo_fop_poll,
	.unlocked_ioctl = syncboss_stream_ioctl
};

static const struct file_operations control_fops = {
	.open = syncboss_control_open,
	.release = miscfifo_fop_release,
	.read = miscfifo_fop_read,
	.poll = miscfifo_fop_poll
};

static const struct file_operations powerstate_fops = {
	.open = syncboss_powerstate_open,
	.release = syncboss_powerstate_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static const struct file_operations nsync_fops = {
	.open = syncboss_nsync_open,
	.release = syncboss_nsync_release,
	.read = miscfifo_fop_read,
	.write = NULL,
	.poll = miscfifo_fop_poll
};

static inline u8 calculate_checksum(const struct syncboss_transaction *trans, size_t len)
{
	const u8 *buf = (u8 *)trans;
	u8 x = 0, sum = 0;

	for (x = 0; x < len; ++x)
		sum += buf[x];
	return 0 - sum;
}

/*
 * Snoop the write and perform any actions necessary before sending the message
 * on to the MCU.
 *
 * This *MUST* be called atomically with enqueueing the snooped packed in queue_tx_packet()
 * (ie. before releasing msg_queue_lock).
 */
static void syncboss_snoop_write_locked(struct syncboss_dev_data *devdata, u8 packet_type)
{
	switch (packet_type) {
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

static unsigned long copy_buffer(void *dest, const void *src, size_t count, bool from_user)
{
	if (from_user)
		return copy_from_user(dest, src, count);

	memcpy(dest, src, count);
	return 0;
}

/*
 * Queue a packet for transmission to the MCU.
 * Packets queued before the kthread has been created will be queued until it has been.
 * Packets queued while streaming is stopped will wake the kthread and be sent immediately.
 * Packets in the queue will be discarded when streaming stops.
 * Responses to any command packets will not be received until the thread is woken (ex.
 * streaming is started).
 */
static ssize_t queue_tx_packet(struct syncboss_dev_data *devdata, const void *buf, size_t count, bool from_user)
{
	int status = 0;
	struct syncboss_msg *smsg = NULL;
	struct spi_transfer *spi_xfer = NULL;
	void *dest;
	u8 packet_type;

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
		if (smsg->transaction_bytes + count <= devdata->transaction_length) {
			dest = smsg->tx.data.raw_data + smsg->transaction_bytes
					- sizeof(struct transaction_header);
			if (copy_buffer(dest, buf, count, from_user)) {
				dev_err(&devdata->spi->dev, "copy_buffer to existing smsg failed!\n");
				mutex_unlock(&devdata->msg_queue_lock);
				return -EINVAL;
			}
			smsg->transaction_bytes += count;

			packet_type = ((struct syncboss_data *)dest)->type;
			syncboss_snoop_write_locked(devdata, packet_type);

			mutex_unlock(&devdata->msg_queue_lock);

			return count;
		}
		smsg = NULL;
	}
	mutex_unlock(&devdata->msg_queue_lock);

	/*
	 * If we're here, we can't combine transactions. Create a new one, in
	 * its own message.
	 */
	if (devdata->msg_queue_item_count > MAX_MSG_QUEUE_ITEMS) {
		dev_warn_ratelimited(&devdata->spi->dev, "msg queue is full\n");
		status = -EBUSY;
		goto err;
	}

	smsg = kzalloc(sizeof(*smsg), GFP_KERNEL | GFP_DMA);
	if (!smsg) {
		status = -ENOMEM;
		goto err;
	}

	dest = &smsg->tx.data.raw_data;
	if (copy_buffer(dest, buf, count, from_user)) {
		dev_err(&devdata->spi->dev, "copy_buffer failed!\n");
		status = -EINVAL;
		goto err;
	}
	smsg->transaction_bytes = sizeof(smsg->tx.header) + count;
	smsg->tx.header.magic_num = SPI_TX_DATA_MAGIC_NUM;

	spi_xfer = &smsg->spi_xfer;
	spi_xfer->tx_buf = &smsg->tx;
	spi_xfer->rx_buf = devdata->rx_elem->buf;
	spi_xfer->len = devdata->transaction_length;
	spi_xfer->bits_per_word = 8;
	spi_xfer->speed_hz = devdata->spi_max_clk_rate;

	spi_message_init(&smsg->spi_msg);
	spi_message_add_tail(spi_xfer, &smsg->spi_msg);
	smsg->spi_msg.spi = devdata->spi;

	/* Enqueue the new message */
	mutex_lock(&devdata->msg_queue_lock);
	/* Check the count again, this time under the lock in case it changed. */
	if (devdata->msg_queue_item_count > MAX_MSG_QUEUE_ITEMS) {
		dev_warn(&devdata->spi->dev, "msg queue is now full\n");
		status = -EBUSY;
		goto err_locked;
	}
	list_add_tail(&smsg->list, &devdata->msg_queue_list);
	devdata->msg_queue_item_count++;

	packet_type = ((struct syncboss_data *)dest)->type;
	syncboss_snoop_write_locked(devdata, packet_type);

	if (devdata->worker) {
		if (devdata->max_msg_send_delay_ns == 0 || devdata->msg_queue_item_count > 1) {
			/*
			 * We are supposed to send immediately, or have more than a full
			 * transaction waiting to be sent. Wake the SPI thread up.
			 */
			hrtimer_try_to_cancel(&devdata->send_timer);
			wake_up_process(devdata->worker);
		} else if (!hrtimer_is_queued(&devdata->send_timer)) {
			/*
			 * Wait up to max_msg_send_delay_ns for more data, to try to
			 * fill at least one transaction.
			 */
			ktime_t reltime = ktime_set(0, devdata->max_msg_send_delay_ns);

			hrtimer_start_range_ns(&devdata->send_timer, reltime,
					SYNCBOSS_SCHEDULING_SLOP_NS, HRTIMER_MODE_REL);
		}
	}

	mutex_unlock(&devdata->msg_queue_lock);

	return count;

err_locked:
	mutex_unlock(&devdata->msg_queue_lock);
err:
	kfree(smsg);
	return status;
}

static int distribute_packet(struct syncboss_dev_data *devdata,
		const struct syncboss_data *packet,
		struct transaction_context *ctx)
{
	const struct syncboss_driver_data_header_t header = {
		.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
		.header_length = sizeof(struct syncboss_driver_data_header_t),
		.from_driver = false
	};

	/*
	 * Stream packets are known by their zero sequence id.  All
	 * other packets are control packets
	 */
	struct miscfifo *fifo_to_use = NULL;
	const char *fifo_name;
	int status = 0;
	size_t payload_size = sizeof(*packet) + packet->data_len;
	bool should_wake;
	struct uapi_pkt_t uapi_pkt;

	/*
	 * Send to direct channel first, no need to make copies, and this will be done in the direct channel copy.
	 * return
	 *  0 - no error and packet not sent to direct buffer, continue to direct channel
	 *  >0 - packet sent to at leats one direct buffer.
	 *  -ENOTSUPP - kernel not configconfigured to support direct channel.
	 *  All other errors are failures to send to direct channel
	 */
	status = syncboss_direct_channel_send_buf(devdata, packet);
	if (status > 0) {
		return 0; /* packet was sent on a direct channel, so don't send to miscfifo */
	} else if (status < 0 && status != -ENOTSUPP) {
		dev_dbg_ratelimited(&devdata->spi->dev, "direct channel distibute failed error %d", status);
		return status;
	}

	if (packet->sequence_id == 0) {
		switch (packet->type) {
		case SYNCBOSS_PROXSTATE_MESSAGE_TYPE:
			signal_powerstate_event(devdata,
					  packet->data[0] ?
					  SYNCBOSS_PROX_EVENT_PROX_ON :
					  SYNCBOSS_PROX_EVENT_PROX_OFF);
			return 0;
		case SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE:
			signal_powerstate_event(devdata, packet->data[0]);
			return 0;
		default:
			break;
		}

		fifo_to_use = &devdata->stream_fifo;
		fifo_name = "stream";
	} else {
		fifo_to_use = &devdata->control_fifo;
		fifo_name = "control";
	}

	if (payload_size > ARRAY_SIZE(uapi_pkt.payload)) {
		dev_warn_ratelimited(&devdata->spi->dev,
				     "%s rx packet is too big [id=%d seq=%d sz=%d]",
				     fifo_name,
				     (int) packet->type,
				     (int) packet->sequence_id,
				     (int) packet->data_len);
		return -EINVAL;
	}

	/* arrange |header|payload| in a single buffer */
	uapi_pkt.header = header;
	memcpy(uapi_pkt.payload, packet, payload_size);

	status = miscfifo_write_buf(fifo_to_use, (u8 *)&uapi_pkt,
			sizeof(uapi_pkt.header) + payload_size, &should_wake);

	if (should_wake) {
		if (fifo_to_use == &devdata->stream_fifo)
			ctx->stream_fifo_wake_needed = true;
		else
			ctx->control_fifo_wake_needed = true;
	}

	if (status < 0)
		dev_warn_ratelimited(&devdata->spi->dev, "uapi %s fifo error (%d)",
				     fifo_name, status);
	return status;
}

static bool recent_reset_event(struct syncboss_dev_data *devdata)
{
	s64 current_time_ms = ktime_get_ms();

	if ((current_time_ms - devdata->last_reset_time_ms) <=
	    SYNCBOSS_RESET_SPI_SETTLING_TIME_MS)
		return true;
	return false;
}

static int spi_nrf_sanity_check_trans(struct syncboss_dev_data *devdata, struct transaction_context *ctx)
{
	int status = 0;
	const struct syncboss_transaction *trans = (struct syncboss_transaction *)devdata->rx_elem->buf;
	u8 checksum;
	bool bad_magic = false;
	bool bad_checksum = false;

	switch (trans->header.magic_num) {
		case SPI_RX_DATA_MAGIC_NUM_IRQ_MODE:
		case SPI_RX_DATA_MAGIC_NUM_POLLING_MODE:
			checksum = calculate_checksum(trans, devdata->transaction_length);
			bad_checksum = checksum != 0;
			if (bad_checksum)
				++devdata->stats.num_bad_checksums;
			break;
		default:
			bad_magic = true;
			++devdata->stats.num_bad_magic_numbers;
			break;
	}

	if (!bad_magic && !bad_checksum)
		return 0;

	devdata->stats.num_invalid_transactions_received++;
	/*
	 * If we get the MCU busy magic number, we'll let the caller decide if/when
	 * it might want to handle retries in a special way
	 */
	if (trans->header.magic_num == SPI_RX_DATA_MAGIC_NUM_MCU_BUSY)
		status = -EAGAIN;
	else
		status = -EIO;

	/*
	 * In IRQ mode, if the we perform a transaction with the MCU,
	 * without being triggered to do so by a data ready IRQ,
	 * the MCU may return a 'busy' value (0xcacacaca). This happens
	 * if transaction occurs just as the MCU happens to be staging
	 * its own message to go out. Don't log warnings in this
	 * case. This is expected. The command will be retried.
	 */
	if (devdata->use_irq_mode && ctx->msg_to_send &&
	    !ctx->data_ready_irq_had_fired &&
	    trans->header.magic_num == SPI_RX_DATA_MAGIC_NUM_MCU_BUSY) {
		return status;
	}

	/*
	 * To avoid noise in the log, skip logging errors if we
	 * have recently reset the mcu
	 */
	if (recent_reset_event(devdata))
		return status;

	dev_err_ratelimited(&devdata->spi->dev,
		"bad %s from MCU (value: 0x%08x, data_ready_irq: %d, wake_timer: %d, send_timer: %d, msg_to_send: %d)",
		bad_magic ? "magic" : (bad_checksum ? "checksum" : "message"),
		bad_magic ? trans->header.magic_num : (bad_checksum ? checksum : 0),
		ctx->data_ready_irq_had_fired,
		ctx->wake_timer_had_fired,
		ctx->send_timer_had_fired,
		ctx->msg_to_send);

	return status;
}

static void query_wake_reason(struct syncboss_dev_data *devdata)
{
	u8 message_buf[sizeof(struct syncboss_data) + 1] = {};
	struct syncboss_data *message = (struct syncboss_data *)message_buf;
	size_t data_len;

	message->type = SYNCBOSS_GET_DATA_MESSAGE_TYPE;
	message->sequence_id = 0;
	message->data_len = 1;
	message->data[0] = SYNCBOSS_WAKEUP_REASON_MESSAGE_TYPE;

	data_len = sizeof(struct syncboss_data) + message->data_len;
	queue_tx_packet(devdata, message, data_len, /* from_user */ false);
}

/*
 * Process received data from MCU and distribute messages are appropriate.
 */
static void process_rx_data(struct syncboss_dev_data *devdata,
			    struct transaction_context *ctx,
			    const struct syncboss_timing *timing)
{
	struct rx_history_elem *rx_elem = devdata->rx_elem;
	const struct syncboss_transaction *trans = (struct syncboss_transaction *) rx_elem->buf;
	const struct syncboss_data *current_packet;
	const uint8_t *trans_end;

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

	rx_elem->transaction_start_time_ns = timing->prev_trans_start_time_ns;
	rx_elem->transaction_end_time_ns = timing->prev_trans_end_time_ns;
	rx_elem->transaction_length = devdata->transaction_length;
	rx_elem->rx_ctr = atomic64_inc_return(&devdata->transaction_ctr);
	if (unlikely(rx_elem->rx_ctr == 1)) {
		/*
		 * Check if the MCU FW supports IRQ-triggered transactions. We can detect this
		 * based on the magic number it is using in the messages it sends.
		 *
		 * Older MCU FW that doesn't support IRQ mode needs to have its wakeup reason
		 * queried explicitly also. Do that here. IRQ-mode firmware will send the
		 * wakeup reason automatically.
		*/
		if (trans->header.magic_num == SPI_RX_DATA_MAGIC_NUM_IRQ_MODE) {
			dev_info(&devdata->spi->dev, "IRQ mode supported by MCU");
			devdata->use_irq_mode = true;
		} else {
			dev_info(&devdata->spi->dev, "IRQ mode not supported- falling back to polling");
			query_wake_reason(devdata);
		}
	}

	/* There's noting to do if there's no data in the packet. */
	if (trans->data.type == 0) {
		devdata->stats.num_empty_transactions_received++;
		return;
	}

	/*
	 * Iterate over the packets and distribute them to either the
	 * stream or control fifos
	 */
	current_packet = &trans->data.syncboss_data;
	trans_end = (uint8_t *)trans + devdata->transaction_length;
	/*
	 * Assumes data is after the last element of current_packet that needs to be
	 * present for parsing the packet.
	 */
	while (((const uint8_t *)current_packet->data <= trans_end) && (current_packet->type != 0)) {
		/*
		 * Sanity check that the packet is entirely contained
		 * within the rx buffer and doesn't overflow
		 */
		const uint8_t *pkt_end = current_packet->data + current_packet->data_len;

		if (pkt_end > trans_end) {
			dev_err_ratelimited(&devdata->spi->dev, "data packet overflow");
			break;
		}
		distribute_packet(devdata, current_packet, ctx);
		/* Next packet */
		current_packet = (struct syncboss_data *)pkt_end;
	}
}

/*
 * Mark FIFO as runnable if new data has been written.
 * They will wake on the next reschedule.
 */
static void wake_readers_sync(struct syncboss_dev_data *devdata,
			      struct transaction_context *ctx)
{
	if (!ctx->stream_fifo_wake_needed && !ctx->control_fifo_wake_needed)
		return;

	preempt_disable();
	if (ctx->stream_fifo_wake_needed) {
		miscfifo_wake_waiters_sync(&devdata->stream_fifo);
		ctx->stream_fifo_wake_needed = false;
	}
	if (ctx->control_fifo_wake_needed) {
		miscfifo_wake_waiters_sync(&devdata->control_fifo);
		ctx->control_fifo_wake_needed = false;
	}
	preempt_enable_no_resched();
}

/*
 * Return true if either a data_ready interrupt has fired or the wake timer
 * has expired since the last transaction. This indicates the MCU should be
 * ready to handle a transaction from us.
 */
static inline bool okay_to_transact(struct syncboss_dev_data *devdata)
{
	return devdata->data_ready_fired || devdata->wake_timer_fired;
}

static inline void reset_timer_status_flags(struct syncboss_dev_data *devdata)
{
	devdata->data_ready_fired = false;
	devdata->send_timer_fired = false;
	devdata->wake_timer_fired = false;
	mb();
}

static void handle_wakeup(struct syncboss_dev_data *devdata)
{
	if (devdata->wakeup_handled)
		return;

	devdata->last_reset_time_ms = ktime_get_ms();

	/*
	 * Wake up any thread that might be waiting on power state
	 * transitions
	 */
	if (devdata->has_prox)
		signal_powerstate_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_UP);

	devdata->wakeup_handled = true;
}

static enum hrtimer_restart wake_timer_callback(struct hrtimer *timer)
{
	struct syncboss_dev_data *devdata =
		container_of(timer, struct syncboss_dev_data, wake_timer);

	devdata->wake_timer_fired = true;

	if (devdata->worker)
		wake_up_process(devdata->worker);

	return HRTIMER_NORESTART;
}

static enum hrtimer_restart send_timer_callback(struct hrtimer *timer)
{
	struct syncboss_dev_data *devdata =
		container_of(timer, struct syncboss_dev_data, send_timer);

	devdata->send_timer_fired = true;

	if (devdata->worker)
		wake_up_process(devdata->worker);

	return HRTIMER_NORESTART;
}

static u64 calc_mcu_ready_sleep_delay_ns(struct syncboss_dev_data *devdata,
			       const struct syncboss_timing *t,
			       bool msg_to_send)
{
	u64 setup_requirement_ns = 0;
	u64 polling_requirement_ns = 0;
	u64 now_ns;
	u64 time_since_prev_trans_end_ns;

	now_ns = ktime_get_boottime_ns();
	time_since_prev_trans_end_ns = now_ns - t->prev_trans_end_time_ns;

	if (time_since_prev_trans_end_ns < t->min_time_between_trans_ns)
		setup_requirement_ns = t->min_time_between_trans_ns - time_since_prev_trans_end_ns;

	if (!devdata->use_irq_mode && !msg_to_send) {
		u64 time_since_prev_trans_start_ns = now_ns - t->prev_trans_start_time_ns;

		if (time_since_prev_trans_start_ns < t->trans_period_ns)
			polling_requirement_ns = t->trans_period_ns - time_since_prev_trans_start_ns;
	}

	return max(setup_requirement_ns, polling_requirement_ns);
}

/*
 * Sleep the spi_transfer_thread if the message queue is empty,
 * waiting until a messages to be sent is enqueued. Must be
 * called from the spi_transfer_thread.
 */
static int sleep_if_msg_queue_empty(struct syncboss_dev_data *devdata,
				struct transaction_context *ctx)
{
	mutex_lock(&devdata->msg_queue_lock);
	if (list_empty(&devdata->msg_queue_list)) {
		/*
		 * Prepare to sleep.
		 * See set_current_state() comment in sched.h explanation of this pattern.
		 *
		 * Regarding release of msg_queue_lock:
		 * queue_tx_packet holds the msg_queue_lock while queueing a packet and
		 * waking this SPI transfer thread. Make sure we set the task state under
		 * the protection of that same lock, so we don't stomp on a parallel
		 * attempt by queue_tx_packet() to wake this thread.
		 */
		set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&devdata->msg_queue_lock);

		if (okay_to_transact(devdata)) {
			/* Abort sleep attempt. It's time for a transaction. */
			__set_current_state(TASK_RUNNING);
			return 0;
		}
		wake_readers_sync(devdata, ctx);
		schedule();

		if (signal_pending(current))
			return -EINTR;
	} else {
		mutex_unlock(&devdata->msg_queue_lock);
	}

	return 0;
}

/*
 * Wait for the MCU to be ready for its next transaction.
 */
static int sleep_or_schedule_if_needed(struct syncboss_dev_data *devdata,
				 const struct syncboss_timing *timing,
				 struct transaction_context *ctx)
{
	bool send_needed, start_timer;
	ktime_t reltime;

	while (!okay_to_transact(devdata)) {
		start_timer = false;

		/*
		 * If we have a message to send, sleep until a data ready IRQ or
		 * the wakeup timer wakes us up, indicating enough time has passed
		 * since the previous transaction to safely send out another one.
		 *
		 * If using legacy polling mode, set up the wake timer for when
		 * the next poll is due.
		 */
		mutex_lock(&devdata->msg_queue_lock);
		send_needed = ctx->msg_to_send || devdata->msg_queue_item_count > 0;
		if (send_needed || !devdata->use_irq_mode) {
			u64 delay_ns;

			/* Calculate how long to wait */
			delay_ns = calc_mcu_ready_sleep_delay_ns(devdata, timing, ctx->msg_to_send);
			if (delay_ns == 0) {
				mutex_unlock(&devdata->msg_queue_lock);
				break;
			}

			/* Really short delays aren't worth context switching for. */
			if (delay_ns < SYNCBOSS_SCHEDULING_SLOP_NS) {
				mutex_unlock(&devdata->msg_queue_lock);
				ndelay(delay_ns);
				break;
			}

			reltime = ktime_set(0, delay_ns);
			start_timer = true;
		}

		/*
		 * Prepare to sleep.
		 * See set_current_state() comment in sched.h explanation of this pattern.
		 *
		 * Regarding release of msg_queue_lock:
		 * queue_tx_packet holds the msg_queue_lock while queueing a packet and
		 * waking this SPI transfer thread. Make sure we set the task state under
		 * the protection of that same lock, so we don't stomp on a parallel
		 * attempt by queue_tx_packet() to wake this thread.
		 */
		set_current_state(TASK_INTERRUPTIBLE);
		mutex_unlock(&devdata->msg_queue_lock);

		if (okay_to_transact(devdata)) {
			/* Abort sleep attempt. Data is already available to read. */
			__set_current_state(TASK_RUNNING);
			break;
		}

		/* Okay, we've committed to sleeping... */

		/*
		 * If we have messages to send, schedule a timer to wake up us as soon
		 * as soon as they are eligible to be sent.
		 */
		if (start_timer) {
			hrtimer_start_range_ns(&devdata->wake_timer, reltime,
					SYNCBOSS_SCHEDULING_SLOP_NS, HRTIMER_MODE_REL);
		}

		/* ZZZzzz... */
		wake_readers_sync(devdata, ctx);
		schedule();

		/* Cancel the hrtimer, in case it's not what woke us */
		if (start_timer)
			hrtimer_cancel(&devdata->wake_timer);

		if (signal_pending(current))
			return -EINTR;
	}

	return 0;
}

static int syncboss_spi_transfer_thread(void *ptr)
{
	int status;
	struct spi_device *spi = (struct spi_device *)ptr;
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);
	struct transaction_context ctx = { 0 };
	struct syncboss_timing timing = { 0 };
	u32 spi_max_clk_rate = 0;
	u16 transaction_length = 0;
	bool use_fastpath;
	struct sched_param sched_settings = {
		.sched_priority = devdata->poll_prio
	};

	status = sched_setscheduler(devdata->worker, SCHED_FIFO, &sched_settings);
	if (status) {
		dev_warn(&spi->dev, "failed to set SCHED_FIFO. (%d)", status);
	}

	spi_max_clk_rate = devdata->spi_max_clk_rate;
	transaction_length = devdata->transaction_length;
	timing.trans_period_ns = devdata->next_stream_settings.trans_period_ns;
	timing.min_time_between_trans_ns =
		devdata->next_stream_settings.min_time_between_trans_ns;
	devdata->max_msg_send_delay_ns =
		devdata->next_stream_settings.max_msg_send_delay_ns;
	use_fastpath = devdata->use_fastpath;

	dev_dbg(&spi->dev, "Entering SPI transfer loop");
	dev_dbg(&spi->dev, "  Max Clk Rate                : %d Hz",
		 spi_max_clk_rate);
	dev_dbg(&spi->dev, "  Transaction Length          : %d",
		 transaction_length);
	dev_dbg(&spi->dev, "  Min time between cmd trans. : %llu us",
		timing.min_time_between_trans_ns / 1000);
	dev_dbg(&spi->dev, "  Max command send delay      : %d us",
		devdata->max_msg_send_delay_ns / 1000);
	dev_dbg(&spi->dev, "  Trans period for legacy mode: %llu us",
		timing.trans_period_ns / 1000);
	dev_dbg(&spi->dev, "  Fastpath enabled            : %s",
		use_fastpath ? "yes" : "no");

	while (likely(!kthread_should_stop())) {
		/*
		 * Find a message to send if we don't have one yet. ctx.msg_to_send may already be
		 * true here in the event of a transaction retry.
		 */
		if (!ctx.msg_to_send) {
			/*
			 * Optimization: use mutex_trylock() to avoid sleeping while we wait for
			 *   for potentially low-priority userspace threads to finish enqueueing
			 *   messages. If a writer is in the process of enqueueing a message, it's
			 *   better to send the 'default_smsg' than wait for the lock to be released.
			 *
			 * Assumptions:
			 *   1) Low SPI read latency and consistent periodicity is more important
			 *      than write latency.
			 *   2) Writes (messages from the msg_queue_list) are relatively infrequent,
			 *      so starvation of messages from the msg_queue_list will not occur.
			 */
			if (mutex_trylock(&devdata->msg_queue_lock)) {
				ctx.smsg = list_first_entry_or_null(&devdata->msg_queue_list, struct syncboss_msg, list);
				if (ctx.smsg) {
					list_del(&ctx.smsg->list);
					devdata->msg_queue_item_count--;
					ctx.smsg->tx.header.checksum = calculate_checksum(&ctx.smsg->tx, transaction_length);
					ctx.msg_to_send = true;
				}
				mutex_unlock(&devdata->msg_queue_lock);
			}
			if (!ctx.msg_to_send)
				ctx.smsg = devdata->default_smsg;
		}

		/*
		 * If streaming is stopped and there's no queued message to send,
		 * go to sleep until something is queued, this thread is woken
		 * by a data_ready IRQ, or it is woken to be stopped.
		 */
		if (!devdata->is_streaming && !devdata->data_ready_fired && !ctx.msg_to_send) {
			status = sleep_if_msg_queue_empty(devdata, &ctx);
			if (status == -EINTR) {
				dev_err(&spi->dev, "SPI thread received signal while waiting for message to send. Stopping.");
				break;
			}

			/*
			 * We may have woken up for a reason besides the send timer.
			 * Try to cancel the timer to avoid it firing unnecessarily.
			 */
			hrtimer_try_to_cancel(&devdata->send_timer);

			/* We're awake! Handle any newly queued messages. */
			continue;
		}

		ctx.smsg->spi_xfer.speed_hz = spi_max_clk_rate;
		ctx.smsg->spi_xfer.len = transaction_length;

		if (use_fastpath && ctx.msg_to_send) {
			if (ctx.prepared_smsg && ctx.smsg != ctx.prepared_smsg) {
				status = devdata->spi_prepare_ops.unprepare_message(spi->master, &ctx.prepared_smsg->spi_msg);
				if (status) {
					dev_err(&spi->dev, "failed to unprepare msg: %d", status);
					break;
				}
				ctx.prepared_smsg = NULL;
			}

			if (!ctx.prepared_smsg) {
				status = devdata->spi_prepare_ops.prepare_message(spi->master, &ctx.smsg->spi_msg);
				if (status) {
					dev_err(&spi->dev, "failed to prepare msg: %d", status);
					break;
				}
				ctx.prepared_smsg = ctx.smsg;
			}
			spi->master->cur_msg = &ctx.smsg->spi_msg;
		}

		status = sleep_or_schedule_if_needed(devdata, &timing, &ctx);
		if (status == -EINTR) {
			dev_err(&spi->dev, "SPI thread received signal while waiting for MCU ready. Stopping.");
			break;
		}

		if (devdata->use_irq_mode && !devdata->data_ready_fired && !ctx.msg_to_send) {
			/*
			 * We're in IRQ mode but a data ready IRQ has not fired since our last
			 * transaction, and we have nothing to send the MCU. This means we likely
			 * woke up because a new message became available after smsg was set at the
			 * top of this loop. Loop around in an attempt to grab that message.
			 */
			continue;
		}

		/*
		 * We're about to send our next transaction. In case we've not yet woken up
		 * miscfifo readers for the previous transaction, do so now.
		 *
		 * To avoid scheduling thrash, we try to avoid waking up readers until we
		 * anyways need to sleep. That way, the woken threads won't preempt us.
		 * Our next sleep will be while waiting for the next SPI transaction to
		 * complete.
		 */
		wake_readers_sync(devdata, &ctx);

#if defined(CONFIG_DYNAMIC_DEBUG)
		if (ctx.msg_to_send) {
			/*
			 * See "logging notes" section at the top of this
			 * file
			 */
			print_hex_dump_bytes("syncboss sending: ",
					    DUMP_PREFIX_OFFSET, &ctx.smsg->tx,
					    transaction_length);
		}
#endif

		ctx.data_ready_irq_had_fired = devdata->data_ready_fired;
		ctx.send_timer_had_fired = devdata->send_timer_fired;
		ctx.wake_timer_had_fired = devdata->wake_timer_fired;
		reset_timer_status_flags(devdata);
		timing.prev_trans_start_time_ns = ktime_get_boottime_ns();
		if (devdata->use_fastpath)
			status = spi_fastpath_transfer(spi, &ctx.smsg->spi_msg);
		else
			status = spi_sync_locked(spi, &ctx.smsg->spi_msg);
		timing.prev_trans_end_time_ns = ktime_get_boottime_ns();

		if (status != 0) {
			dev_err(&spi->dev, "spi_sync_locked failed with error %d", status);

			if (status == -ETIMEDOUT) {
				/* Retry if needed and keep going */
				continue;
			}

			break;
		}

		devdata->stats.num_transactions++;
		if (!ctx.msg_to_send)
			devdata->stats.num_empty_transactions_sent++;

		/* Check response was not malformed. Retry if so. */
		status = spi_nrf_sanity_check_trans(devdata, &ctx);
		if (status < 0) {
			/*
			 * We raced the MCU wrt issuing a transaction while it was prepping one.
			 * Try again.
			 */
			if ((status == -EAGAIN) && ctx.data_ready_irq_had_fired)
				devdata->data_ready_fired = true;

			continue;
		}

		/* Process the received data */
		process_rx_data(devdata, &ctx, &timing);

		/* Cleanup, to prepare for the next message */
		if (ctx.msg_to_send) {
			if (ctx.prepared_smsg) {
				BUG_ON(ctx.smsg != ctx.prepared_smsg);
				devdata->spi_prepare_ops.unprepare_message(spi->master, &ctx.prepared_smsg->spi_msg);
				spi->master->cur_msg = NULL;
				ctx.prepared_smsg = NULL;
			}
			kfree(ctx.smsg);
		}

		/* Grab a new message on the next iteration of the loop */
		ctx.msg_to_send = false;
	}

	if (ctx.prepared_smsg) {
		devdata->spi_prepare_ops.unprepare_message(spi->master, &ctx.prepared_smsg->spi_msg);
		spi->master->cur_msg = NULL;
	}

	/*
	 * Wait until thread is told to stop.
	 * See set_current_state() comment in sched.h explanation of this pattern.
	 */
	for (;;) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (kthread_should_stop())
			break;
		schedule();
	}
	__set_current_state(TASK_RUNNING);

	dev_dbg(&spi->dev, "SPI transfer thread stopped");
	return 0;
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
			 "not pushing prox cal since it's invalid");
		return;
	}

	if (enable)
		dev_info(&devdata->spi->dev,
			 "pushing prox cal to device and enabling prox wakeup");
	else
		dev_info(&devdata->spi->dev, "disabling prox wakeup");

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
	if (gpio_is_valid(devdata->gpio_ready)) {
		if (gpio_get_value(devdata->gpio_ready) == 1)
			return true;

		/*
		 * The GPIO is low, but make sure it stays low for at
		 * least a couple uS to be sure the MCU's GPIO isn't in
		 * the middle of a high-low-high transition for
		 * triggering an IRQ.
		 */
		udelay(5);
		if (gpio_get_value(devdata->gpio_ready) == 0)
			return false;
	}

	return true;
}

static int wait_for_syncboss_wake_state(struct syncboss_dev_data *devdata,
					bool awake)
{
	const s64 starttime = ktime_get_ms();
	const s64 deadline = starttime + SYNCBOSS_SLEEP_WAKE_TIMEOUT_WITH_GRACE_MS;
	s64 now;
	s64 elapsed;
	const char *awakestr = awake ? "awake" : "asleep";
	s32 *stat = awake ? &devdata->stats.last_awake_dur_ms : &devdata->stats.last_asleep_dur_ms;

	dev_info(&devdata->spi->dev, "waiting for syncboss to be %s", awakestr);
	for (now = starttime; now < deadline; now = ktime_get_ms()) {
		if (is_mcu_awake(devdata) == awake) {
			elapsed = ktime_get_ms() - starttime;
			if (elapsed > SYNCBOSS_SLEEP_WAKE_TIMEOUT_MS) {
				WARN(true, "MCU %s took too long (after %lldms)",
				     awakestr, elapsed);
			} else {
				dev_info(&devdata->spi->dev,
					 "MCU is %s (after %lldms)",
					 awakestr, elapsed);

			}

			*stat = elapsed;
			return 0;
		}
		msleep(20);
	}

	elapsed = ktime_get_ms() - starttime;
	WARN(true, "MCU failed to %s within %lldms.", awakestr, elapsed);
	*stat = -1;
	return -ETIMEDOUT;
}

static int create_default_smsg_locked(struct syncboss_dev_data *devdata)
{
	struct syncboss_msg *default_smsg;
	struct spi_transfer *spi_xfer;

	default_smsg = kzalloc(sizeof(*devdata->default_smsg), GFP_KERNEL | GFP_DMA);
	if (!default_smsg)
		return -ENOMEM;

	default_smsg->tx.header.magic_num = SPI_TX_DATA_MAGIC_NUM;
	default_smsg->tx.header.checksum = calculate_checksum(&default_smsg->tx,
						devdata->transaction_length);

	spi_xfer = &default_smsg->spi_xfer;
	spi_xfer->tx_buf = &default_smsg->tx;
	spi_xfer->rx_buf = devdata->rx_elem->buf;
	spi_xfer->len = devdata->transaction_length;
	spi_xfer->bits_per_word = 8;
	spi_message_init(&default_smsg->spi_msg);
	spi_message_add_tail(spi_xfer, &default_smsg->spi_msg);
	default_smsg->spi_msg.spi = devdata->spi;

	devdata->default_smsg = default_smsg;

	return 0;
}

static void destroy_default_smsg_locked(struct syncboss_dev_data *devdata)
{
	kfree(devdata->default_smsg);
	devdata->default_smsg = NULL;
}

static int send_dummy_smsg(struct syncboss_dev_data *devdata)
{
	int status;
	struct spi_transfer *spi_xfer;
	struct syncboss_msg *dummy_smsg;

	dummy_smsg = kzalloc(sizeof(*dummy_smsg), GFP_KERNEL | GFP_DMA);
	if (!dummy_smsg)
		return -ENOMEM;

	spi_xfer = &dummy_smsg->spi_xfer;
	spi_xfer->tx_buf = &dummy_smsg->tx;
	spi_xfer->len = devdata->transaction_length;
	spi_xfer->bits_per_word = 8;
	spi_xfer->speed_hz = devdata->spi_max_clk_rate;
	spi_message_init(&dummy_smsg->spi_msg);
	spi_message_add_tail(spi_xfer, &dummy_smsg->spi_msg);
	dummy_smsg->spi_msg.spi = devdata->spi;

	spi_bus_lock(devdata->spi->master);
	status = devdata->spi_prepare_ops.prepare_transfer_hardware(devdata->spi->master);
	if (status)
		goto error;

	if (devdata->use_fastpath) {
		status = devdata->spi_prepare_ops.prepare_message(devdata->spi->master, &dummy_smsg->spi_msg);
		if (status)
			goto error_after_prepare_hw;

		devdata->spi->master->cur_msg = &dummy_smsg->spi_msg;
		status = spi_fastpath_transfer(devdata->spi, &dummy_smsg->spi_msg);

		devdata->spi_prepare_ops.unprepare_message(devdata->spi->master, &dummy_smsg->spi_msg);
		devdata->spi->master->cur_msg = NULL;
	} else {
		status = spi_sync_locked(devdata->spi, &dummy_smsg->spi_msg);
	}

error_after_prepare_hw:
	devdata->spi_prepare_ops.unprepare_transfer_hardware(devdata->spi->master);
error:
	spi_bus_unlock(devdata->spi->master);
	kfree(dummy_smsg);
	return status;
}

static void override_spi_prepare_ops(struct syncboss_dev_data *devdata)
{
	struct spi_device *spi = devdata->spi;

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
}

static void restore_spi_prepare_ops(struct syncboss_dev_data *devdata)
{
	struct spi_device *spi = devdata->spi;

	if (devdata->use_fastpath) {
		spi->master->prepare_message = devdata->spi_prepare_ops.prepare_message;
		spi->master->unprepare_message = devdata->spi_prepare_ops.unprepare_message;
	}
	spi->master->prepare_transfer_hardware = devdata->spi_prepare_ops.prepare_transfer_hardware;
	spi->master->unprepare_transfer_hardware = devdata->spi_prepare_ops.unprepare_transfer_hardware;
}

static int wake_mcu_by_pin_reset(struct syncboss_dev_data *devdata)
{
	syncboss_pin_reset(devdata);
	return wait_for_syncboss_wake_state(devdata, true);
}

static int wake_mcu_by_dummy_spi(struct syncboss_dev_data *devdata)
{
	int status;

	dev_info(&devdata->spi->dev, "sending dummy SPI transaction to wake MCU");

	status = send_dummy_smsg(devdata);
	if (status) {
		dev_err(&devdata->spi->dev, "failed to send dummy SPI transaction: %d", status);
		return status;
	}

	return wait_for_syncboss_wake_state(devdata, true);
}

static int wake_mcu(struct syncboss_dev_data *devdata, bool force_pin_reset)
{
	int status = 0;
	bool mcu_awake = is_mcu_awake(devdata);

	if (mcu_awake && !force_pin_reset)
		return 0;

	dev_info(&devdata->spi->dev,
		"attempting to wake MCU due to mcu_awake=%d, force_pin_reset=%d",
		mcu_awake, force_pin_reset);

	/* Reset "okay to transact" flags so no transactions occur until MCU wakes */
	reset_timer_status_flags(devdata);

	/* Clear flag so that handle_wakeup() will run on next MCU wakeup */
	devdata->wakeup_handled = false;

	/* Assume legacy polling mode until messaging from MCU proves otherwise. */
	devdata->use_irq_mode = false;

	/*
	 * If supported, attempt to wake the MCU by sending it a dummy SPI
	 * transaction. The MCU will wake on CS toggle.
	 *
	 * Otherwise, or if that fails, attempt a pin reset.
	 */
	if (devdata->has_wake_on_spi && !force_pin_reset) {
		status = wake_mcu_by_dummy_spi(devdata);
		if (status) {
			dev_warn(&devdata->spi->dev, "MCU failed to wake on SPI. Falling back to pin reset.");
			status = wake_mcu_by_pin_reset(devdata);
		}
	} else {
		status = wake_mcu_by_pin_reset(devdata);
	}

	if (status)
		return status;

	/* Wake up was successful. */
	handle_wakeup(devdata);

	return 0;
}

static int start_streaming_locked(struct syncboss_dev_data *devdata)
{
	int status = 0;
	struct task_struct *worker;

	if (devdata->is_streaming) {
		dev_warn(&devdata->spi->dev, "streaming already started");
		return 0;
	}

	if (devdata->hall_sensor) {
		status = regulator_enable(devdata->hall_sensor);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"failed to enable hall sensor power: %d",
					status);
			goto error;
		}
	}

	if (devdata->mcu_core) {
		status = regulator_enable(devdata->mcu_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"failed to enable MCU power: %d",
					status);
			goto error;
		}
	}

	if (devdata->imu_core) {
		status = regulator_enable(devdata->imu_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"failed to enable IMU power: %d",
					status);
			goto error;
		}
	}

	if (devdata->mag_core) {
		status = regulator_enable(devdata->mag_core);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"failed to enable mag power: %d",
					status);
			goto error;
		}
	}

	if (devdata->rf_amp) {
		status = regulator_enable(devdata->rf_amp);
		if (status < 0) {
			dev_err(&devdata->spi->dev,
					"failed to enable rf amp power: %d",
					status);
			goto error;
		}
	}

	/*
	 * Take ownership of calling prepare/unprepare away from the SPI
	 * framework, so can manage calls more efficiently for our usecase.
	 */
	override_spi_prepare_ops(devdata);

	status = wake_mcu(devdata, devdata->force_reset_on_open);
	if (status)
		goto error_after_spi_ops_update;

	if (devdata->next_stream_settings.transaction_length == 0) {
		dev_err(&devdata->spi->dev,
				"transaction length has not yet been set");
		status = -EINVAL;
		goto error_after_spi_ops_update;
	}

	devdata->transaction_length = devdata->next_stream_settings.transaction_length;
	devdata->spi_max_clk_rate = devdata->next_stream_settings.spi_max_clk_rate;
	status = create_default_smsg_locked(devdata);
	if (status)
		goto error_after_spi_ops_update;

	devdata->force_reset_on_open = false;

	dev_info(&devdata->spi->dev, "starting stream");

	/*
	 * Hold the SPI bus lock when streaming, so we can use spi_sync_locked() and
	 * avoid per-transaction lock acquisition.
	 */
	spi_bus_lock(devdata->spi->master);

	status = devdata->spi_prepare_ops.prepare_transfer_hardware(devdata->spi->master);
	if (status)
		goto error_after_spi_bus_lock;

	atomic64_set(&devdata->transaction_ctr, 0);

	if (devdata->use_fastpath) {
		status = devdata->spi_prepare_ops.prepare_message(devdata->spi->master, &devdata->default_smsg->spi_msg);
		if (status)
			goto error_after_prepare_transfer_hardware;
	}

	/*
	 * The worker thread should not be running yet, so the worker
	 * should always be null here
	 */
	BUG_ON(devdata->worker != NULL);

	worker = kthread_create(syncboss_spi_transfer_thread,
					 devdata->spi, "syncboss:spi_thread");
	if (IS_ERR(worker)) {
		status = PTR_ERR(worker);
		dev_err(&devdata->spi->dev, "failed to start SPI transfer kernel thread. (%d)",
			status);
		goto error_after_prepare_msg;
	}

	dev_dbg(&devdata->spi->dev, "setting SPI transfer thread cpu affinity: %*pb",
		cpumask_pr_args(&devdata->cpu_affinity));
	kthread_bind_mask(worker, &devdata->cpu_affinity);

	devdata->is_streaming = true;
	devdata->worker = worker;
	wake_up_process(devdata->worker);

	push_prox_cal_and_enable_wake(devdata, devdata->powerstate_events_enabled);

	return 0;

error_after_prepare_msg:
	if (devdata->use_fastpath)
		devdata->spi_prepare_ops.unprepare_message(devdata->spi->master, &devdata->default_smsg->spi_msg);
error_after_prepare_transfer_hardware:
	devdata->spi_prepare_ops.unprepare_transfer_hardware(devdata->spi->master);
error_after_spi_bus_lock:
	spi_bus_unlock(devdata->spi->master);
	destroy_default_smsg_locked(devdata);
error_after_spi_ops_update:
	restore_spi_prepare_ops(devdata);
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

	dev_dbg(&devdata->spi->dev, "telling MCU to go to sleep");

	/* Send command to put the MCU to sleep */
	queue_tx_packet(devdata, &message, sizeof(message), /* from_user */ false);

	/*
	 * Wait for shutdown. The gpio_ready line will go low when things are
	 * fully shut down
	 */
	is_asleep = (wait_for_syncboss_wake_state(devdata,
						  /*awake*/false) == 0);
	if (!is_asleep) {
		dev_err(&devdata->spi->dev,
			"MCU failed to sleep within %dms. pin-reset will be forced on stream start.",
			SYNCBOSS_SLEEP_WAKE_TIMEOUT_WITH_GRACE_MS);
			devdata->force_reset_on_open = true;
	}
}

static void syncboss_on_camera_release(struct syncboss_dev_data *devdata)
{
	dev_dbg(&devdata->spi->dev, "turning off cameras");

	devdata->cameras_enabled = false;
}

static void stop_streaming_locked(struct syncboss_dev_data *devdata)
{
	int status;
	struct syncboss_msg *smsg, *temp_smsg;

	if (!devdata->is_streaming) {
		dev_warn(&devdata->spi->dev, "streaming already stopped");
		return;
	}

	dev_info(&devdata->spi->dev, "stopping stream");
	devdata->is_streaming = false;

	/* Tell syncboss to go to sleep */
	shutdown_syncboss_mcu_locked(devdata);

	if (devdata->cameras_enabled) {
		dev_warn(&devdata->spi->dev,
			"cameras still enabled during stream stop. forcing camera release.");
		syncboss_on_camera_release(devdata);
	}

	kthread_stop(devdata->worker);
	hrtimer_cancel(&devdata->wake_timer);
	hrtimer_cancel(&devdata->send_timer);
	devdata->worker = NULL;
	/*
	 * Now that device comm is totally down, signal the SYSTEM_DOWN
	 * event
	 */
	if (devdata->has_prox)
		signal_powerstate_event(devdata, SYNCBOSS_PROX_EVENT_SYSTEM_DOWN);

	if (devdata->use_fastpath)
		devdata->spi_prepare_ops.unprepare_message(devdata->spi->master, &devdata->default_smsg->spi_msg);

	devdata->spi_prepare_ops.unprepare_transfer_hardware(devdata->spi->master);

	spi_bus_unlock(devdata->spi->master);

	/* Discard and free and pending messages that were to be sent */
	mutex_lock(&devdata->msg_queue_lock);
	list_for_each_entry_safe(smsg, temp_smsg, &devdata->msg_queue_list, list) {
		list_del(&smsg->list);
		--devdata->msg_queue_item_count;
		kfree(smsg);
	}
	BUG_ON(devdata->msg_queue_item_count != 0);
	mutex_unlock(&devdata->msg_queue_lock);

	destroy_default_smsg_locked(devdata);

	/*
	 * Clear any unread fifo events that userspace has not yet read, so these
	 * don't get confused for messages from the new streaming session once
	 * streaming is resumed.
	 */
	miscfifo_clear(&devdata->stream_fifo);
	miscfifo_clear(&devdata->control_fifo);

	/* Return prepare/unprepare call ownership to the kernel SPI framework. */
	restore_spi_prepare_ops(devdata);

	if (devdata->rf_amp) {
		status = regulator_disable(devdata->rf_amp);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"failed to disable rf amp power: %d",
					status);
		}
	}

	if (devdata->mcu_core) {
		status = regulator_disable(devdata->mcu_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"failed to disable MCU power: %d",
					status);
		}
	}

	if (devdata->imu_core) {
		status = regulator_disable(devdata->imu_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"failed to disable IMU power: %d",
					status);
		}
	}

	if (devdata->mag_core) {
		status = regulator_disable(devdata->mag_core);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"failed to disable mag power: %d",
					status);
		}
	}

	if (devdata->hall_sensor) {
		status = regulator_disable(devdata->hall_sensor);
		if (status < 0) {
			dev_warn(&devdata->spi->dev,
					"failed to disable hall sensor power: %d",
					status);
		}
	}
}

static void powerstate_set_enable_locked(struct syncboss_dev_data *devdata, bool enable)
{
	int status = 0;
	bool must_wake_syncboss = !devdata->is_streaming;

	if (must_wake_syncboss) {
		/*
		 * We're about to wake up syncboss, and then shut it
		 * down again.  If we don't silence the powerstate messages,
		 * the user will get a confusing stream of messages
		 * during this process.
		 */
		devdata->silence_all_powerstate_events = true;

		status = start_streaming_locked(devdata);
		if (status)
			goto error;
	}

	push_prox_cal_and_enable_wake(devdata, enable);

	devdata->powerstate_events_enabled = enable;

error:
	/*
	 * Note: We can stop streaming immediately since the stop
	 * streaming impl sends a sleep command to SyncBoss and waits
	 * for it to sleep.  This means our prox settings are ensured
	 * to get over as well.
	 */
	if (must_wake_syncboss)
		stop_streaming_locked(devdata);

	devdata->silence_all_powerstate_events = false;
}

static void powerstate_enable_locked(struct syncboss_dev_data *devdata)
{
	powerstate_set_enable_locked(devdata, true);
}

static void powerstate_disable_locked(struct syncboss_dev_data *devdata)
{
	powerstate_set_enable_locked(devdata, false);
}

static irqreturn_t isr_data_ready(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;

	devdata->data_ready_fired = true;

	if (!devdata->use_irq_mode) {
		/*
		 * When using legacy polling mode, this ISR will only fire if the
		 * MCU has rebooted. Reset the transaction counter to 0 so that
		 * process_rx_data() will query the wake-up reason again.
		 */
		atomic64_set(&devdata->transaction_ctr, 0);
	}

	if (unlikely(!devdata->wakeup_handled)) {
		/* MCU just woke up, so set the last_reset_time_ms */
		devdata->last_reset_time_ms = ktime_get_ms();

		/*
		 * Hold the wake-lock for a short period of time (which should
		 * allow time for clients to open a handle to syncboss)
		 */
		pm_wakeup_event(&devdata->spi->dev, SYNCBOSS_WAKEUP_EVENT_DURATION_MS);
	}

	if (devdata->worker) {
		hrtimer_try_to_cancel(&devdata->wake_timer);
		wake_up_process(devdata->worker);
	}

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

	devdata->spi = spi;

	devdata->streaming_client_count = 0;

	/* T114417103: for old sysfs sequence number mechanism */
	devdata->next_avail_seq_num = 1;
	/* T114417103: for new ioctl sequence number mechanism */
	client_data_init_locked(devdata);
	/*
	 * syncboss_sequence_number_reset_locked does some unnecessary initilization
	 * as devdata is zero initilized
	 */
	devdata->last_seq_num = SYNCBOSS_SEQ_NUM_MAX;

	devdata->next_stream_settings.trans_period_ns = SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS;
	devdata->next_stream_settings.min_time_between_trans_ns =
		SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS;
	devdata->next_stream_settings.max_msg_send_delay_ns =
		SYNCBOSS_DEFAULT_MAX_MSG_SEND_DELAY_NS;
	devdata->next_stream_settings.spi_max_clk_rate = SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE;
	devdata->poll_prio = SYNCBOSS_DEFAULT_POLL_PRIO;
	atomic64_set(&devdata->transaction_ctr, 0);

	devdata->prox_canc = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdl = INVALID_PROX_CAL_VALUE;
	devdata->prox_thdh = INVALID_PROX_CAL_VALUE;
	devdata->prox_config_version = DEFAULT_PROX_CONFIG_VERSION_VALUE;
	devdata->powerstate_last_evt = INVALID_POWERSTATE_VALUE;

	devdata->use_fastpath = of_property_read_bool(node, "oculus,syncboss-use-fastpath");

	/* this will be overwritten by init through syncboss_sysfs */
	cpumask_setall(&devdata->cpu_affinity);
	dev_dbg(&devdata->spi->dev, "initial SPI thread cpu affinity: %*pb\n",
		cpumask_pr_args(&devdata->cpu_affinity));

	init_completion(&devdata->pm_resume_completion);
	complete_all(&devdata->pm_resume_completion);

	devdata->syncboss_pm_workqueue = create_singlethread_workqueue("syncboss_pm_workqueue");

	hrtimer_init(&devdata->wake_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	devdata->wake_timer.function = wake_timer_callback;
	hrtimer_init(&devdata->send_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	devdata->send_timer.function = send_timer_callback;

	devdata->gpio_reset = of_get_named_gpio(node,
			"oculus,syncboss-reset", 0);
	devdata->gpio_timesync = of_get_named_gpio(node,
			"oculus,syncboss-timesync", 0);
	devdata->gpio_ready = of_get_named_gpio(node,
			"oculus,syncboss-wakeup", 0);
	devdata->gpio_nsync = of_get_named_gpio(node,
			"oculus,syncboss-nsync", 0);

	devdata->has_prox = of_property_read_bool(node, "oculus,syncboss-has-prox");
	devdata->has_no_prox_cal = of_property_read_bool(node, "oculus,syncboss-has-no-prox-cal");
	devdata->has_wake_on_spi = of_property_read_bool(node, "oculus,syncboss-has-wake-on-spi");
	devdata->has_seq_num_ioctl = of_property_read_bool(node, "oculus,syncboss-has-seq-num-ioctl");

	dev_dbg(&devdata->spi->dev,
		 "GPIOs: reset: %d, timesync: %d, wakeup/ready: %d, nsync: %d",
		 devdata->gpio_reset, devdata->gpio_timesync,
		 devdata->gpio_ready, devdata->gpio_nsync);

	dev_dbg(&devdata->spi->dev, "has-prox: %s",
		 devdata->has_prox ? "true" : "false");
	if (devdata->gpio_reset < 0) {
		dev_err(&devdata->spi->dev,
			"reset GPIO was not specificed in the device tree. MCU reset and firmware updates will fail.");
	}

	mutex_init(&devdata->state_mutex);
	spin_lock_init(&devdata->nsync_lock);

	return 0;
}

static int syncboss_probe(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata;
	struct device *dev = &spi->dev;

	int status = 0;

	dev_dbg(dev, "probing");
	dev_dbg(
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

	status = init_syncboss_dev_data(devdata, spi);
	if (status < 0)
		goto error;

	devdata->rx_elem = devm_kzalloc(dev, sizeof(*devdata->rx_elem),
					GFP_KERNEL | GFP_DMA);
	if (!devdata->rx_elem) {
		status = -ENOMEM;
		goto error_after_devdata_init;
	}

	mutex_init(&devdata->msg_queue_lock);
	INIT_LIST_HEAD(&devdata->msg_queue_list);

	devm_fw_init_regulator(dev, &devdata->hall_sensor, "hall-sensor");
	devm_fw_init_regulator(dev, &devdata->mcu_core, "mcu-core");
	devm_fw_init_regulator(dev, &devdata->imu_core, "imu-core");
	devm_fw_init_regulator(dev, &devdata->mag_core, "mag-core");
	devm_fw_init_regulator(dev, &devdata->rf_amp, "rf-amp");

	dev_set_drvdata(dev, devdata);

	/* misc dev stuff */
	devdata->misc.name = SYNCBOSS_DEVICE_NAME;
	devdata->misc.minor = MISC_DYNAMIC_MINOR;
	devdata->misc.fops = &fops;

	status = misc_register(&devdata->misc);
	if (status < 0) {
		dev_err(dev, "%s failed to register misc device, error %d",
			__func__, status);
		goto error_after_devdata_init;
	}

	devdata->stream_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	devdata->stream_fifo.config.filter_fn = should_send_stream_packet;
	status = devm_miscfifo_register(dev, &devdata->stream_fifo);
	if (status < 0) {
		dev_err(dev, "%s failed to register miscfifo device, error %d",
			__func__, status);
		goto error_after_misc_register;
	}

	devdata->misc_stream.name = SYNCBOSS_STREAM_DEVICE_NAME;
	devdata->misc_stream.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_stream.fops = &stream_fops;

	status = misc_register(&devdata->misc_stream);
	if (status < 0) {
		dev_err(dev, "%s failed to register misc device, error %d",
			__func__, status);
		goto error_after_misc_register;
	}

	devdata->control_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	status = devm_miscfifo_register(dev, &devdata->control_fifo);
	if (status < 0) {
		dev_err(dev, "%s failed to register miscfifo device, error %d",
			__func__, status);
		goto error_after_stream_register;
	}

	devdata->misc_control.name = SYNCBOSS_CONTROL_DEVICE_NAME;
	devdata->misc_control.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_control.fops = &control_fops;

	status = misc_register(&devdata->misc_control);
	if (status < 0) {
		dev_err(dev, "%s failed to register misc device, error %d",
			__func__, status);
		goto error_after_stream_register;
	}

	devdata->powerstate_fifo.config.kfifo_size = SYNCBOSS_MISCFIFO_SIZE;
	status = devm_miscfifo_register(dev, &devdata->powerstate_fifo);
	if (status < 0) {
		dev_err(dev,
			"%s failed to register miscfifo device, error %d",
			__func__, status);
		goto error_after_control_register;
	}

	devdata->misc_powerstate.name = SYNCBOSS_POWERSTATE_DEVICE_NAME;
	devdata->misc_powerstate.minor = MISC_DYNAMIC_MINOR;
	devdata->misc_powerstate.fops = &powerstate_fops;

	status = misc_register(&devdata->misc_powerstate);
	if (status < 0) {
		dev_err(dev, "%s failed to register misc device, error %d",
			__func__, status);
		goto error_after_control_register;
	}

#if defined(CONFIG_SYNCBOSS_DIRECTCHANNEL)
	status = syncboss_register_direct_channel_interface(devdata,dev);
	if (status < 0) {
		dev_err(dev, "%s failed to register direct channel misc device, error %d",
			__func__, status);
		goto error_after_control_register;
	}
#endif /* CONFIG_SYNCBOSS_DIRECTCHANNEL */
	/*
	 * Configure the MCU pin reset as an output, and leave it asserted
	 * until the syncboss device is opened by userspace.
	 */
	gpio_direction_output(devdata->gpio_reset, 0);

	/* Set up TE timestamp before sysfs nodes get enabled */
	atomic64_set(&devdata->last_te_timestamp_ns, ktime_to_ns(ktime_get()));

	if (gpio_is_valid(devdata->gpio_nsync)) {
		devdata->nsync_fifo.config.kfifo_size = SYNCBOSS_NSYNC_MISCFIFO_SIZE;
		status = devm_miscfifo_register(dev, &devdata->nsync_fifo);

		devdata->misc_nsync.name = SYNCBOSS_NSYNC_DEVICE_NAME;
		devdata->misc_nsync.minor = MISC_DYNAMIC_MINOR;
		devdata->misc_nsync.fops = &nsync_fops;

		status = misc_register(&devdata->misc_nsync);
		if (status < 0) {
			dev_err(dev, "failed to register nsync misc device, error %d",
					status);
		}
	}

	/*
	 * Create child SWD fw-update devices, if any
	 */
	status = of_platform_populate(dev->of_node, oculus_swd_match_table,
				      NULL, dev);
	if (status < 0)
		goto error_after_powerstate_register;

	status = syncboss_init_sysfs_attrs(devdata);
	if (status < 0)
		goto error_after_of_platform_populate;

	/* Init interrupts */
	if (devdata->gpio_ready < 0) {
		dev_err(dev, "device tree is missing 'syncboss-ready' GPIO");
		status = devdata->gpio_ready;
		goto error_after_sysfs;
	}
	devdata->ready_irq = gpio_to_irq(devdata->gpio_ready);
	irq_set_status_flags(devdata->ready_irq, IRQ_DISABLE_UNLAZY);
	/* This irq must be able to wake up the system */
	irq_set_irq_wake(devdata->ready_irq, /*on*/ 1);
	status = devm_request_irq(dev, devdata->ready_irq,
				  isr_data_ready,
				  IRQF_TRIGGER_RISING,
				  devdata->misc.name, devdata);
	if (status < 0) {
		dev_err(dev, "failed to get data ready irq, error %d", status);
		goto error_after_sysfs;
	}

	if (devdata->gpio_timesync >= 0) {
		devdata->timesync_irq = gpio_to_irq(devdata->gpio_timesync);
		status = devm_request_irq(&spi->dev, devdata->timesync_irq,
				timesync_irq_handler,
				IRQF_TRIGGER_RISING,
				devdata->misc.name, devdata);
		if (status < 0) {
			dev_err(dev, "failed to get timesync irq, error %d", status);
			goto error_after_sysfs;
		}
	}

	status = syncboss_debugfs_init(devdata);
	if (status && -ENODEV != status) {
		dev_err(dev, "failed to init debugfs: %d", status);
		return status;
	}
	/*
	 * Init device as a wakeup source (so wake interrupts can hold
	 * the wake lock)
	 */
	device_init_wakeup(dev, /*enable*/ true);

	return 0;

error_after_sysfs:
	syncboss_deinit_sysfs_attrs(devdata);
error_after_of_platform_populate:
	of_platform_depopulate(&spi->dev);
error_after_powerstate_register:
	misc_deregister(&devdata->misc_powerstate);
error_after_control_register:
	misc_deregister(&devdata->misc_control);
error_after_stream_register:
	misc_deregister(&devdata->misc_stream);
error_after_misc_register:
	misc_deregister(&devdata->misc);
error_after_devdata_init:
	destroy_workqueue(devdata->syncboss_pm_workqueue);
error:
	return status;
}

static int syncboss_remove(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata = NULL;

	devdata = (struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);

	flush_workqueue(devdata->syncboss_pm_workqueue);
	destroy_workqueue(devdata->syncboss_pm_workqueue);

	of_platform_depopulate(&spi->dev);
	syncboss_deinit_sysfs_attrs(devdata);

	syncboss_debugfs_deinit(devdata);

	misc_deregister(&devdata->misc_stream);
	misc_deregister(&devdata->misc_control);
	misc_deregister(&devdata->misc_powerstate);
	misc_deregister(&devdata->misc);

#if defined(CONFIG_SYNCBOSS_DIRECTCHANNEL)
	syncboss_deregister_direct_channel_interface(devdata);
#endif /* CONFIG_SYNCBOSS_DIRECTCHANNEL */

	return 0;
}

static void syncboss_on_camera_probe(struct syncboss_dev_data *devdata)
{
	dev_dbg(&devdata->spi->dev, "turning on cameras");

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

#ifdef CONFIG_PM_SLEEP
struct syncboss_resume_work_data {
	struct work_struct work;
	struct syncboss_dev_data *devdata;
};

static int syncboss_suspend(struct device *dev)
{
	struct syncboss_dev_data *devdata =
		(struct syncboss_dev_data *)dev_get_drvdata(dev);

	if (!completion_done(&devdata->pm_resume_completion)) {
		int ret;

		dev_warn(dev, "trying to suspend before previous resume has completed. waiting for that to finish.\n");
		ret = wait_for_completion_timeout(&devdata->pm_resume_completion,
			msecs_to_jiffies(SYNCBOSS_SLEEP_WAKE_TIMEOUT_MS));
		if (ret <= 0) {
			dev_err(dev, "aborting suspend due unexpectedly-long in-progress resume.\n");
			return -EBUSY;
		}
	}

	if (!mutex_trylock(&devdata->state_mutex)) {
		dev_info(dev, "aborting suspend due to in-progress syncboss state change.\n");
		return -EBUSY;
	}

	if (devdata->streaming_client_count > 0) {
		dev_info(dev, "stopping streaming for system suspend");

		if (devdata->powerstate_last_evt == SYNCBOSS_PROX_EVENT_PROX_ON) {
			/*
			 * Handle the case where we suspend while the prox is still covered (ex. device
			 * is put into suspend by a button press, or goes into suspend after a long period
			 * of no motion).
			 *
			 * In this scenario, a PROX_ON event will be emitted when we start streaming and
			 * re-enable MCU prox events upon resume from suspend, even if the prox sensor
			 * state hasn't physically changed.
			 *
			 * To prevent such a PROX_ON from waking the whole system and turning on the
			 * display, etc, swallow all PROX_ON events received until at least one
			 * PROX_OFF event has been reported, indicating the persistent obstruction has
			 * been cleared.
			 */
			dev_dbg(dev, "ignoring prox_on events until the next prox_off");
			devdata->eat_prox_on_events = true;
		}

		stop_streaming_locked(devdata);
	}
	mutex_unlock(&devdata->state_mutex);

	return 0;
}

static void do_syncboss_resume_work(struct work_struct *work)
{
	struct syncboss_resume_work_data *wd = container_of(work, struct syncboss_resume_work_data, work);
	struct syncboss_dev_data *devdata = wd->devdata;
	struct device *dev = &devdata->spi->dev;
	int status;

	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	if (devdata->streaming_client_count > 0) {
		dev_info(dev, "resuming streaming after system suspend");
		status = start_streaming_locked(devdata);
		if (status)
			dev_err(dev, "%s: failed to resume streaming (%d)", __func__, status);
	}

	 // Release mutex acquired by syncboss_resume()
	mutex_unlock(&devdata->state_mutex);

	complete_all(&devdata->pm_resume_completion);

	devm_kfree(dev, wd);
}

static int syncboss_resume(struct device *dev)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)dev_get_drvdata(dev);
	struct syncboss_resume_work_data *wd;

	wd = devm_kmalloc(dev, sizeof(*wd), GFP_KERNEL);
	if (!wd)
		return -ENOMEM;

	reinit_completion(&devdata->pm_resume_completion);

	wd->devdata = devdata;
	INIT_WORK(&wd->work, do_syncboss_resume_work);

	/*
	 * Hold mutex until do_syncboss_resume_work completes to prevent userspace
	 * from changing state before then.
	 */
	mutex_lock(&devdata->state_mutex);
	queue_work(devdata->syncboss_pm_workqueue, &wd->work);

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(syncboss_pm_ops, syncboss_suspend, syncboss_resume);

/* SPI Driver Info */
struct spi_driver syncboss_spi_driver = {
	.driver = {
		.name = "syncboss_spi",
		.owner = THIS_MODULE,
		.of_match_table = syncboss_spi_table,
		.pm = &syncboss_pm_ops,
		.probe_type = PROBE_PREFER_ASYNCHRONOUS
	},
	.probe	= syncboss_probe,
	.remove = syncboss_remove
};

static int __init syncboss_init(void)
{
	return spi_register_driver(&syncboss_spi_driver);
}

static void __exit syncboss_exit(void)
{
	spi_unregister_driver(&syncboss_spi_driver);
}

module_init(syncboss_init);
module_exit(syncboss_exit);
MODULE_DESCRIPTION("SYNCBOSS");
MODULE_LICENSE("GPL v2");
