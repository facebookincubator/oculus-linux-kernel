// SPDX-License-Identifier: GPL-2.0
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/pm.h>
#include <linux/regulator/consumer.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/time64.h>
#include <linux/version.h>
#include <linux/wait.h>

#include "../fw_helpers.h"

#include "syncboss_spi.h"
#include "syncboss_spi_debugfs.h"
#include "syncboss_spi_fastpath.h"
#include "syncboss_spi_sequence_number.h"

#ifdef CONFIG_OF
static const struct of_device_id syncboss_spi_table[] = {
	{ .compatible = "meta,syncboss-spi" },
	{ },
};
static const struct of_device_id syncboss_subdevice_match_table[] = {
	{ .compatible = "meta,syncboss-consumer" },
	{ },
};
#else
	#define syncboss_spi_table NULL
	#define syncboss_subdevice_match_table NULL
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

static int wake_mcu(struct syncboss_dev_data *devdata, bool force_pin_reset);
static int start_streaming_locked(struct syncboss_dev_data *devdata);
static void stop_streaming_locked(struct syncboss_dev_data *devdata);

static inline s64 ktime_get_ms(void)
{
	return ktime_to_ms(ktime_get());
}

/* Increment refcount used to keep the MCU awake. */
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

/* Decrement refcount used to keep the MCU awake. */
static void syncboss_dec_mcu_client_count_locked(struct syncboss_dev_data *devdata)
{
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->mcu_client_count < 1);

	if (--devdata->mcu_client_count == 0) {
		dev_info(&devdata->spi->dev, "asserting MCU reset");
		gpio_set_value(devdata->gpio_reset, 0);
	}
}

/* Increment refcount used to start streaming, waking the MCU if needed. */
static int syncboss_inc_streaming_client_count_locked(struct syncboss_dev_data *devdata)
{
	int status = 0;

	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->streaming_client_count < 0);

	if (devdata->streaming_client_count == 0) {
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_STARTING, NULL);
		syncboss_inc_mcu_client_count_locked(devdata);

		dev_dbg(&devdata->spi->dev, "starting streaming thread");
		status = start_streaming_locked(devdata);
		if (status) {
			syncboss_dec_mcu_client_count_locked(devdata);
			return status;
		}
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_STARTED, NULL);
	}

	++devdata->streaming_client_count;

	return status;
}

/* Decrement refcount used to start streaming. */
static int syncboss_dec_streaming_client_count_locked(struct syncboss_dev_data *devdata)
{
	/* Note: Must be called under the state_mutex lock! */
	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	BUG_ON(devdata->streaming_client_count < 1);

	--devdata->streaming_client_count;
	if (devdata->streaming_client_count == 0) {
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_STOPPING, NULL);

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

		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_STOPPED, NULL);
	}
	return 0;
}

/* Create and initialize a client_data entry */
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

	data->index = devdata->client_data_index++;
	status = syncboss_debugfs_client_add_locked(devdata, data);
	if (status && -ENODEV != status)
		dev_warn(dev, "failed to add client data to debugfs for %d: %d",
			data->task->pid, status);

	list_add_tail(&data->list_entry, &devdata->client_data_list);

	*client_data = data;

	return 0;
}

/* Retrieve a client_data entry */
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

/* Remove and free a client_data entry */
static void client_data_destroy_locked(struct syncboss_dev_data *devdata,
	struct syncboss_client_data *client_data)
{
	struct device *dev = &devdata->spi->dev;

	syncboss_debugfs_client_remove_locked(devdata, client_data);

	list_del(&client_data->list_entry);

	syncboss_sequence_number_release_client_locked(devdata, client_data);

	devm_kfree(dev, client_data);
}

/* SYNCBOSS_SEQUENCE_NUMBER_ALLOCATE_IOCTL handler */
static long syncboss_ioctl_handle_seq_num_allocate_locked(
	struct syncboss_dev_data *devdata, struct syncboss_client_data *client_data,
	unsigned long arg)
{
	struct device *dev = &devdata->spi->dev;
	long status;
	long status_release;
	uint8_t seq;

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

/* SYNCBOSS_SEQUENCE_NUMBER_RELEASE_IOCTL handler */
static long syncboss_ioctl_handle_seq_num_release_locked(
	struct syncboss_dev_data *devdata, struct syncboss_client_data *client_data,
	unsigned long arg)
{
	struct device *dev = &devdata->spi->dev;
	long status;
	uint8_t seq;

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

/* Check is a child SWD device is performing a FW update */
static int fw_update_check_busy(struct device *dev, void *data)
{
	struct device_node *node = dev_of_node(dev);

	if (of_device_is_compatible(node, "meta,swd")) {
		struct swd_dev_data *devdata = dev_get_drvdata(dev);

		if (!devdata) {
			dev_err(dev, "swd device not yet available: %s", node->name);
			return -ENODEV;
		}

		if (devdata->fw_update_state == FW_UPDATE_STATE_WRITING_TO_HW)
			return -EBUSY;
	}
	return 0;
}

/* Wrapper helper to perform memcpy from either a kernel or user buffer */
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

/* /dev/syncboss0 open handler */
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


/* /dev/syncboss0 release handler */
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

	/*
	 * This must be done before syncboss_dec_streaming_client_count_locked
	 * since it will syncboss_sequence_number_reset_locked if the ref count
	 * goes to zero and reset checks for any remaining clients.
	 */
	client_data_destroy_locked(devdata, client_data);

	syncboss_dec_streaming_client_count_locked(devdata);

	mutex_unlock(&devdata->state_mutex);

	return 0;
}

/* /dev/syncboss0 write handler */
static ssize_t syncboss_write(struct file *filp, const char __user *buf,
			      size_t count, loff_t *f_pos)
{
	struct syncboss_dev_data *devdata =
		container_of(filp->private_data, struct syncboss_dev_data,
			     misc);

	return queue_tx_packet(devdata, buf, count, /* from_user */ true);
}

/* /dev/syncboss0 ioctl handler */
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

/* /dev/syncboss0 file operations */
static const struct file_operations fops = {
	.open = syncboss_open,
	.release = syncboss_release,
	.read = NULL,
	.write = syncboss_write,
	.poll = NULL,
	.unlocked_ioctl = syncboss_ioctl
};

/* Return true if the MCU was reset in the last SYNCBOSS_RESET_SPI_SETTLING_TIME_MS */
static bool recent_reset_event(struct syncboss_dev_data *devdata)
{
	s64 current_time_ms = ktime_get_ms();

	if ((current_time_ms - devdata->last_reset_time_ms) <=
	    SYNCBOSS_RESET_SPI_SETTLING_TIME_MS)
		return true;
	return false;
}

/* Calculate the checksum of a SPI transaction */
static inline u8 calculate_checksum(const struct syncboss_transaction *trans, size_t len)
{
	const u8 *buf = (u8 *)trans;
	u8 x = 0, sum = 0;

	for (x = 0; x < len; ++x)
		sum += buf[x];
	return 0 - sum;
}

/* Validate the data in a SPI transaction. Return 0 if it is okay, or an error otherwise */
static int spi_nrf_sanity_check_trans(struct syncboss_dev_data *devdata, struct transaction_context *ctx)
{
	int status = 0;
	const struct syncboss_transaction *trans = (struct syncboss_transaction *)devdata->rx_elem->buf;
	u8 checksum;
	bool bad_magic = false;
	bool bad_checksum = false;

	switch (trans->header.magic_num) {
	case SPI_RX_DATA_MAGIC_NUM_IRQ_MODE:
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
	 * If we perform a transaction with the MCU, without being triggered to do
	 * so by a data ready IRQ, the MCU may return a 'busy' value (0xcacacaca).
	 * This happens if transaction occurs just as the MCU happens to be staging
	 * its own message to go out. Don't log warnings in this case. This is
	 * expected. The command will be retried.
	 */
	if (ctx->msg_to_send && !ctx->data_ready_irq_had_fired &&
	    trans->header.magic_num == SPI_RX_DATA_MAGIC_NUM_MCU_BUSY) {
		return status;
	}

	/*
	 * To avoid noise in the log, skip logging errors if we have recently
	 * reset the mcu.
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

/* Process received data from MCU and distribute messages are appropriate. */
static void process_rx_data(struct syncboss_dev_data *devdata,
			    struct transaction_context *ctx,
			    const struct syncboss_timing *timing)
{
	struct rx_history_elem *rx_elem = devdata->rx_elem;
	const struct syncboss_transaction *trans = (struct syncboss_transaction *) rx_elem->buf;
	const struct syncboss_data *current_packet;
	const uint8_t *trans_end;

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
		struct rx_packet_info packet_info;

		if (pkt_end > trans_end) {
			dev_err_ratelimited(&devdata->spi->dev, "data packet overflow");
			break;
		}

		packet_info = (struct rx_packet_info) {
			.header = {
				.header_version = SYNCBOSS_DRIVER_HEADER_CURRENT_VERSION,
				.header_length = sizeof(struct syncboss_driver_data_header_t),
				.from_driver = false,
				.nsync_offset_status = SYNCBOSS_TIME_OFFSET_INVALID,
			},
			.data = current_packet
		};

		/* Deliver pack to consumers */
		raw_notifier_call_chain(&devdata->rx_packet_event_chain, current_packet->type, (void *)&packet_info);
		/* Next packet */
		current_packet = (struct syncboss_data *)pkt_end;
	}
}

/* Wake userspace threads that are waiting on data to read  */
static inline void wake_readers_sync(struct syncboss_dev_data *devdata)
{
	raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_WAKE_READERS, NULL);
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

/* Reset all 'timer fired' flags */
static inline void reset_timer_status_flags(struct syncboss_dev_data *devdata)
{
	devdata->data_ready_fired = false;
	devdata->send_timer_fired = false;
	devdata->wake_timer_fired = false;
	/* Ensure flags are observed to be cleared before */
	mb();
}

/* Handle wake timer expiry */
static enum hrtimer_restart wake_timer_callback(struct hrtimer *timer)
{
	struct syncboss_dev_data *devdata =
		container_of(timer, struct syncboss_dev_data, wake_timer);

	devdata->wake_timer_fired = true;

	if (devdata->worker)
		wake_up_process(devdata->worker);

	return HRTIMER_NORESTART;
}

/* Handle sent timer expiry */
static enum hrtimer_restart send_timer_callback(struct hrtimer *timer)
{
	struct syncboss_dev_data *devdata =
		container_of(timer, struct syncboss_dev_data, send_timer);

	devdata->send_timer_fired = true;

	if (devdata->worker)
		wake_up_process(devdata->worker);

	return HRTIMER_NORESTART;
}

/* Calculate how long to sleep the SPI thread for before the next transaction. */
static u64 calc_mcu_ready_sleep_delay_ns(struct syncboss_dev_data *devdata,
			       const struct syncboss_timing *t,
			       bool msg_to_send)
{
	u64 setup_requirement_ns = 0;
	u64 now_ns;
	u64 time_since_prev_trans_end_ns;

	now_ns = ktime_get_boottime_ns();
	time_since_prev_trans_end_ns = now_ns - t->prev_trans_end_time_ns;

	if (time_since_prev_trans_end_ns < t->min_time_between_trans_ns)
		setup_requirement_ns = t->min_time_between_trans_ns - time_since_prev_trans_end_ns;

	return setup_requirement_ns;
}

/*
 * Sleep the spi_transfer_thread if the message queue is empty, waiting until a
 * messages to be sent is enqueued. Must be called from the spi_transfer_thread.
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
		wake_readers_sync(devdata);
		schedule();

		if (signal_pending(current))
			return -EINTR;
	} else {
		mutex_unlock(&devdata->msg_queue_lock);
	}

	return 0;
}

/* Wait for the MCU to be ready for its next transaction */
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
		if (send_needed) {
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
		wake_readers_sync(devdata);
		schedule();

		/* Cancel the hrtimer, in case it's not what woke us */
		if (start_timer)
			hrtimer_cancel(&devdata->wake_timer);

		if (signal_pending(current))
			return -EINTR;
	}

	return 0;
}

/* Main syncboss SPI message processing thread / loop */
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
		.sched_priority = devdata->thread_prio
	};

	status = sched_setscheduler(devdata->worker, SCHED_FIFO, &sched_settings);
	if (status)
		dev_warn(&spi->dev, "failed to set SCHED_FIFO. (%d)", status);

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
			 *   potentially low-priority userspace threads to finish enqueueing
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

		if (!devdata->data_ready_fired && !ctx.msg_to_send) {
			/*
			 * The data ready IRQ has not fired since our last transaction, and we
			 * have nothing to send the MCU. This means we likely woke up because
			 * a new message became available after smsg was set at the top of this
			 * loop. Loop around in an attempt to grab that message.
			 */
			continue;
		}

		/*
		 * We're about to send our next transaction. In case we've not yet woken up
		 * userspace readers for the previous transaction, do so now.
		 *
		 * To avoid scheduling thrash, we try to avoid waking up readers until we
		 * anyways need to sleep. That way, the woken threads won't preempt us.
		 * Our next sleep will be while waiting for the next SPI transaction to
		 * complete.
		 */
		wake_readers_sync(devdata);

#if defined(CONFIG_DYNAMIC_DEBUG)
		if (ctx.msg_to_send) {
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

/*
 * Check if the MCU was woken. If the GPIO is seen to be high, assume it is
 * awake. Require the GPIO to be low for at least 5us as an indication is
 * asleep.
 */
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

/* Wait for the MCU to wake or shut down. */
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

/*
 * Initialize a 'default' message to be sent when a transaction is needed
 * (ex. in order to read from the MCU) but there's nothing of importance to
 * send to MCU.
 */
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

/* Free the default message created by create_default_smsg_locked() */
static void destroy_default_smsg_locked(struct syncboss_dev_data *devdata)
{
	kfree(devdata->default_smsg);
	devdata->default_smsg = NULL;
}

/*
 * For Wake-on-SPI devices only:
 * Send a message to the MCU without starting streaming and discard the
 * data that comes back. The purspose of this function is to generate a
 * SPI CS line toggle, waking the MCU.
 */
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

/* Toggle the MCU pin reset GPIO to force it to (re-)boot. */
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

	raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_MCU_PIN_RESET, NULL);
	devdata->last_reset_time_ms = ktime_get_ms();
	gpio_set_value(devdata->gpio_reset, 1);
}

/*
 * For Wake-on-SPI devices only:
 * Wake the MCU by sending it a SPI transaction, and wait for it to boot.
 */
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


/* Wake the MCU via a pin reset, and wait for it to boot. */
static int wake_mcu_by_pin_reset(struct syncboss_dev_data *devdata)
{
	syncboss_pin_reset(devdata);
	return wait_for_syncboss_wake_state(devdata, true);
}

/*
 * SPI framework optimization: take control of prepare/unprepare calls from the
 * framework, so we can void calling them more than necessary.
 */
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

/* Undo the effects of override_spi_prepare_ops() */
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

/* Wake up the MCU by whatever means it supports, and handle that wake-up one it completes. */
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

	/* Reset flag to indicate a wake attempt is in progress. */
	devdata->wakeup_handled = false;

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
	devdata->last_reset_time_ms = ktime_get_ms();
	raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_MCU_UP, NULL);
	devdata->wakeup_handled = true;

	return 0;
}

/* Shut down the MCU by issueing a SPI command, and wait for it to halt */
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

/* Start the main SPI / message thread processing loop */
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

/* Stop the main SPI thread / message processing loop */
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

	kthread_stop(devdata->worker);
	hrtimer_cancel(&devdata->wake_timer);
	hrtimer_cancel(&devdata->send_timer);
	devdata->worker = NULL;

	raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_MCU_DOWN, NULL);

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

/* Handle a 'data ready to be read' IRQ from the MCU */
static irqreturn_t isr_data_ready(int irq, void *p)
{
	struct syncboss_dev_data *devdata = (struct syncboss_dev_data *)p;

	devdata->data_ready_fired = true;

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


/*  Consumer API: Acquire state_lock. Use with caution! */
static void consumer_syncboss_state_lock(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	mutex_lock(&devdata->state_mutex);
}

/* Consumer API: Release state_lock. Use with caution! */
static void consumer_syncboss_state_unlock(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	mutex_unlock(&devdata->state_mutex);
}

/* Consumer API: Get stream status */
static bool consumer_get_is_streaming(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	return devdata->is_streaming;
}

/* Consumer API: Register for state change events */
static int consumer_state_event_notifier_register(struct device *child, struct notifier_block *nb)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	if (devdata->is_streaming) {
		dev_err(&devdata->spi->dev, "notifiers can't be registered while streaming");
		ret = -EBUSY;
		goto out;
	}
	ret = raw_notifier_chain_register(&devdata->state_event_chain, nb);
out:
	mutex_unlock(&devdata->state_mutex);
	return ret;
}

/* Consumer API: Unregister from change events */
static int consumer_state_event_notifier_unregister(struct device *child, struct notifier_block *nb)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	if (devdata->is_streaming) {
		dev_err(&devdata->spi->dev, "notifiers can't be unregistered while streaming");
		ret = -EBUSY;
		goto out;
	}
	ret = raw_notifier_chain_unregister(&devdata->state_event_chain, nb);
out:
	mutex_unlock(&devdata->state_mutex);
	return ret;
}

/* Consumer API: Register for received messages from MCU */
static int consumer_rx_packet_notifier_register(struct device *child, struct notifier_block *nb)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	ret = raw_notifier_chain_register(&devdata->rx_packet_event_chain, nb);
	mutex_unlock(&devdata->state_mutex);

	return ret;
}

/* Consumer API: Unregister from received messages from MCU */
static int consumer_rx_packet_notifier_unregister(struct device *child, struct notifier_block *nb)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	ret = raw_notifier_chain_unregister(&devdata->rx_packet_event_chain, nb);
	mutex_unlock(&devdata->state_mutex);

	return ret;
}

/* Consumer API: Vote to boot the MCU and keep it awake (refcounted) */
static int consumer_enable_mcu(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	mutex_lock(&devdata->state_mutex);
	syncboss_inc_mcu_client_count_locked(devdata);
	mutex_unlock(&devdata->state_mutex);

	return 0;
}

/* Consumer API: Remove vote for keeping the MCU awake (refcounted) */
static int consumer_disable_mcu(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	mutex_lock(&devdata->state_mutex);
	syncboss_dec_mcu_client_count_locked(devdata);
	mutex_unlock(&devdata->state_mutex);

	return 0;
}

/* Consumer API: Vote to start the SPI data stream (refcounted). Keeps MCU awake. */
static int consumer_enable_stream(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	ret = syncboss_inc_streaming_client_count_locked(devdata);
	mutex_unlock(&devdata->state_mutex);

	return ret;
}

/* Consumer API: Remove vote to keep the SPI data stream running (refcounted). */
static int consumer_disable_stream(struct device *child)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);
	int ret;

	mutex_lock(&devdata->state_mutex);
	ret = syncboss_dec_streaming_client_count_locked(devdata);
	mutex_unlock(&devdata->state_mutex);

	return ret;
}

/*
 * Consumer API: Send a message to the MCU when it is next streaming, or immediately
 * if streaming is active.
 */
static ssize_t consumer_queue_tx_packet(struct device *child, const void *buf, size_t count, bool from_user)
{
	struct syncboss_dev_data *devdata = dev_get_drvdata(child->parent);

	return queue_tx_packet(devdata, buf, count, from_user);
}

/* Probe helper funcion to initialize struct syncboss_dev_data members */
static int init_syncboss_dev_data(struct syncboss_dev_data *devdata,
				   struct spi_device *spi)
{
	struct device_node *node = spi->dev.of_node;

	devdata->spi = spi;

	devdata->streaming_client_count = 0;

	/* For ioctl sequence number mechanism */
	INIT_LIST_HEAD(&devdata->client_data_list);
	devdata->client_data_index = 0;

	/*
	 * syncboss_sequence_number_reset_locked does some unnecessary initialization
	 * as devdata is zero initialized
	 */
	devdata->last_seq_num = SYNCBOSS_SEQ_NUM_MAX;

	devdata->next_stream_settings.trans_period_ns = SYNCBOSS_DEFAULT_TRANSACTION_PERIOD_NS;
	devdata->next_stream_settings.min_time_between_trans_ns =
		SYNCBOSS_DEFAULT_MIN_TIME_BETWEEN_TRANSACTIONS_NS;
	devdata->next_stream_settings.max_msg_send_delay_ns =
		SYNCBOSS_DEFAULT_MAX_MSG_SEND_DELAY_NS;
	devdata->next_stream_settings.spi_max_clk_rate = SYNCBOSS_DEFAULT_SPI_MAX_CLK_RATE;
	devdata->thread_prio = SYNCBOSS_DEFAULT_THREAD_PRIO;
	atomic64_set(&devdata->transaction_ctr, 0);

	devdata->has_wake_on_spi = of_property_read_bool(node, "meta,syncboss-has-wake-on-spi");
	devdata->use_fastpath = of_property_read_bool(node, "meta,syncboss-use-fastpath");

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
			"meta,syncboss-reset", 0);
	devdata->gpio_ready = of_get_named_gpio(node,
			"meta,syncboss-wakeup", 0);

	dev_dbg(&devdata->spi->dev,
		 "GPIOs: reset: %d, wakeup/ready: %d",
		 devdata->gpio_reset, devdata->gpio_ready);

	if (devdata->gpio_reset < 0) {
		dev_err(&devdata->spi->dev,
			"reset GPIO was not specificed in the device tree. MCU reset and firmware updates will fail.");
	}

	devdata->consumer_ops.get_is_streaming = consumer_get_is_streaming;
	devdata->consumer_ops.syncboss_state_lock = consumer_syncboss_state_lock;
	devdata->consumer_ops.syncboss_state_unlock = consumer_syncboss_state_unlock;
	devdata->consumer_ops.state_event_notifier_register = consumer_state_event_notifier_register;
	devdata->consumer_ops.state_event_notifier_unregister = consumer_state_event_notifier_unregister;
	devdata->consumer_ops.rx_packet_notifier_register = consumer_rx_packet_notifier_register;
	devdata->consumer_ops.rx_packet_notifier_unregister = consumer_rx_packet_notifier_unregister;
	devdata->consumer_ops.enable_mcu = consumer_enable_mcu;
	devdata->consumer_ops.disable_mcu = consumer_disable_mcu;
	devdata->consumer_ops.enable_stream = consumer_enable_stream;
	devdata->consumer_ops.disable_stream = consumer_disable_stream;
	devdata->consumer_ops.queue_tx_packet = consumer_queue_tx_packet;

	RAW_INIT_NOTIFIER_HEAD(&devdata->state_event_chain);
	RAW_INIT_NOTIFIER_HEAD(&devdata->rx_packet_event_chain);

	mutex_init(&devdata->state_mutex);

	return 0;
}

/* Platform device initialization */
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

	devdata->misc.name = SYNCBOSS_DEVICE_NAME;
	devdata->misc.minor = MISC_DYNAMIC_MINOR;
	devdata->misc.fops = &fops;

	status = misc_register(&devdata->misc);
	if (status < 0) {
		dev_err(dev, "%s failed to register misc device, error %d",
			__func__, status);
		goto error_after_devdata_init;
	}

	/*
	 * Configure the MCU pin reset as an output, and leave it asserted
	 * until the syncboss device is opened by userspace.
	 */
	gpio_direction_output(devdata->gpio_reset, 0);

	/* Create child devices, if any */
	status = of_platform_populate(dev->of_node, syncboss_subdevice_match_table,
				      NULL, dev);
	if (status < 0)
		goto error_after_misc_reg;

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
error_after_misc_reg:
	misc_deregister(&devdata->misc);
error_after_devdata_init:
	destroy_workqueue(devdata->syncboss_pm_workqueue);
error:
	return status;
}

/* Platform device removal cleanup */
static int syncboss_remove(struct spi_device *spi)
{
	struct syncboss_dev_data *devdata = NULL;

	devdata = (struct syncboss_dev_data *)dev_get_drvdata(&spi->dev);

	flush_workqueue(devdata->syncboss_pm_workqueue);
	destroy_workqueue(devdata->syncboss_pm_workqueue);

	of_platform_depopulate(&spi->dev);
	syncboss_deinit_sysfs_attrs(devdata);

	syncboss_debugfs_deinit(devdata);

	misc_deregister(&devdata->misc);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
struct syncboss_resume_work_data {
	struct work_struct work;
	struct syncboss_dev_data *devdata;
};

/* Handle system suspend by allowing the MCU to shut down */
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
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_SUSPENDING, NULL);
		stop_streaming_locked(devdata);
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_SUSPENDED, NULL);
	}
	mutex_unlock(&devdata->state_mutex);

	return 0;
}

/* Deferred work from syncboss_resume, so system resume doesn't have to wait for it. */
static void do_syncboss_resume_work(struct work_struct *work)
{
	struct syncboss_resume_work_data *wd = container_of(work, struct syncboss_resume_work_data, work);
	struct syncboss_dev_data *devdata = wd->devdata;
	struct device *dev = &devdata->spi->dev;
	int status;

	BUG_ON(!mutex_is_locked(&devdata->state_mutex));
	if (devdata->streaming_client_count > 0) {
		dev_info(dev, "resuming streaming after system suspend");
		raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_RESUMING, NULL);
		status = start_streaming_locked(devdata);
		if (status)
			dev_err(dev, "%s: failed to resume streaming (%d)", __func__, status);
		else
			raw_notifier_call_chain(&devdata->state_event_chain, SYNCBOSS_EVENT_STREAMING_RESUMED, NULL);
	}

	 // Release mutex acquired by syncboss_resume()
	mutex_unlock(&devdata->state_mutex);

	complete_all(&devdata->pm_resume_completion);

	devm_kfree(dev, wd);
}

/* Handle resume from system suspend by restarting the MCU, if appropriate */
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
MODULE_DESCRIPTION("Syncboss SPI communication driver");
MODULE_LICENSE("GPL v2");
