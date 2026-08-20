// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/device.h> // create_class
#include <linux/err.h>
#include <linux/fs.h> // file_operations
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/slab.h> // kzalloc
#include <linux/timekeeping.h> // ktime
#include <linux/uaccess.h> // copy_to_user
#include <linux/wait.h> // wait queue

#include <common/stp_device_logging.h>
#include <common/stp_error_mapping.h>
#include <device/stp_device.h>
#include <stp/common/stp_os.h>
#include <stp/controller/stp_controller.h>

#define CREATE_TRACE_POINTS
#include <device/stp_channel_events.h>

#define STP_DEVICE_NAME "stp"
#define STP_DEV_CHANNEL_COUNT 32
#define STP_XFER_BUFFER_SIZE_BYTES (1024 * 5)
#define STP_WAIT_CHANNEL_TIMEOUT_MS	2000

DEFINE_MUTEX(stp_spi_busy_lock);
static bool spi_busy;

struct stp_device_stats {
	uint64_t total_bytes;
	uint64_t transactions;
	ktime_t total_usecs;
};

struct stp_device_channel {
	dev_t devt;
	struct device *dev;
	struct completion write_done;
	struct completion read_done;
	struct completion fsync_done;
	struct completion open_done;

	wait_queue_head_t poll_event_q;
	wait_queue_head_t inuse_q;

	uint8_t channel;
	uint8_t priority;
  bool closing;

	// These buffer lengths are used for the channel pipeline and are
	// passed in to stp_controller. They are specified in the device tree.
	unsigned int tx_buffer_len;
	unsigned int rx_buffer_len;

	// These buffers are handed to stplib for internal use
	uint8_t *tx_buffer;
	uint8_t *rx_buffer;

	// These buffers are used as a staging ground for
	// transfers during read and write. They are used
	// to store user space data
	uint8_t *write_buffer;
	uint8_t *read_buffer;

	struct mutex inuse_lock;
	struct mutex rx_lock;
	struct mutex tx_lock;
	struct mutex release_lock;

	bool inuse;

	struct stp_device_stats read_stats;
	struct stp_device_stats write_stats;

	bool nonblock;
};

/* STP Device internal data cache */
struct stp_device {
	bool device_ready;
	int major;
	struct device *parent_dev;
	struct class *device_class;

	struct mutex device_ready_lock;

	// Since there can only be 32 channels, just allocate
	// a static array of them for easy access.
	struct stp_device_channel *channels[STP_DEV_CHANNEL_COUNT];
};

// The handle that contains the persistent data for the device class
// and each character device.
static struct stp_device *_stp_device;

bool stp_get_device_ready(void)
{
	bool device_ready;

	if (!_stp_device) {
		STP_DRV_LOG_ERR("NULL stp device pointer");
		return false;
	}

	mutex_lock(&_stp_device->device_ready_lock);
	device_ready = _stp_device->device_ready;
	mutex_unlock(&_stp_device->device_ready_lock);

	return device_ready;
}
EXPORT_SYMBOL(stp_get_device_ready);

static void stp_set_device_ready(bool ready)
{
	if (!_stp_device)
		return;

	mutex_lock(&_stp_device->device_ready_lock);
	_stp_device->device_ready = ready;
	mutex_unlock(&_stp_device->device_ready_lock);
}

static bool validate_channel(uint8_t channel)
{
	if (!stp_get_device_ready())
		return false;

	if (channel >= STP_DEV_CHANNEL_COUNT || !_stp_device->channels[channel]) {
		STP_DRV_LOG_ERR("c%d out of bounds", channel);
		return false;
	}

	return true;
}

void stp_channel_signal_write(uint8_t channel)
{
	if (validate_channel(channel)) {
		trace_stp_channel_signal_write(channel);
		wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
		complete(&_stp_device->channels[channel]->write_done);
	}
}

int stp_channel_wait_write(uint8_t channel) {
  if (validate_channel(channel)) {
    // Check if the channel is being closed
    if (_stp_device->channels[channel]->closing)
      return -ENODEV;

    return wait_for_completion_interruptible(&_stp_device->channels[channel]->write_done);
  }
   return -ENODEV;
}

void stp_channel_signal_read(uint8_t channel)
{
	if (validate_channel(channel)) {
		trace_stp_channel_signal_read(channel);
		wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
		complete(&_stp_device->channels[channel]->read_done);
	}
}

int stp_channel_wait_read(uint8_t channel) {
  if (validate_channel(channel)) {
    // Check if the channel is being closed
    if (_stp_device->channels[channel]->closing)
      return -ENODEV;

    return wait_for_completion_interruptible(&_stp_device->channels[channel]->read_done);
  }
  return -ENODEV;
}
EXPORT_SYMBOL(stp_channel_wait_read);

int stp_channel_wait_read_timeout(uint8_t channel, unsigned long timeout)
{
  if (validate_channel(channel)) {
    // Check if the channel is being closed
    if (_stp_device->channels[channel]->closing)
      return -ENODEV;

    return wait_for_completion_interruptible_timeout(
        &_stp_device->channels[channel]->read_done, timeout);
  }

  return -ENODEV;
}
EXPORT_SYMBOL(stp_channel_wait_read_timeout);

void stp_channel_signal_fsync(uint8_t channel)
{
	if (validate_channel(channel)) {
		trace_stp_channel_signal_fsync(channel);
		complete(&_stp_device->channels[channel]->fsync_done);
	}
}

int stp_channel_wait_fsync(uint8_t channel)
{
  if (validate_channel(channel))
  {
    // Check if the channel is being closed
    if (_stp_device->channels[channel]->closing)
      return -ENODEV;

    return wait_for_completion_interruptible(&_stp_device->channels[channel]->fsync_done);
  }
    return -ENODEV;
  }
EXPORT_SYMBOL(stp_channel_wait_fsync);

void stp_channel_reset_fsync(uint8_t channel)
{
	if (validate_channel(channel)) {
		trace_stp_channel_reset_fsync(channel);
		reinit_completion(&_stp_device->channels[channel]->fsync_done);
	}
}

int stp_channel_wait_open(uint8_t channel) {
  if (validate_channel(channel)) {
    // Check if the channel is being closed
    if (_stp_device->channels[channel]->closing)
      return -ENODEV;

    return wait_for_completion_interruptible(&_stp_device->channels[channel]->open_done);
  }
   return -ENODEV;
}

void stp_channel_signal_open(uint8_t channel)
{
	if (validate_channel(channel)) {
		trace_stp_channel_signal_open(channel);
		complete(&_stp_device->channels[channel]->open_done);
		wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
	}
}

// returns true if lock accquired
static bool stp_set_channel_inuse(struct stp_device_channel *const channel)
{
	bool got_lock = false;

	mutex_lock(&channel->inuse_lock);
	if (!channel->inuse) {
		got_lock = true;
		channel->inuse = true;
	}
	mutex_unlock(&channel->inuse_lock);

	return got_lock;
}

static bool stp_check_channel_inuse(struct stp_device_channel *const channel)
{
	bool inuse = false;

	mutex_lock(&channel->inuse_lock);
	inuse = channel->inuse;
	mutex_unlock(&channel->inuse_lock);

	return inuse;
}

static void stp_unset_channel_inuse(struct stp_device_channel *const channel)
{
	mutex_lock(&channel->inuse_lock);
	trace_stp_channel_unset_inuse(channel->channel);
	channel->inuse = false;
	mutex_unlock(&channel->inuse_lock);
	wake_up_interruptible(&channel->inuse_q);
}

static void stp_wait_and_set_channel_inuse(struct stp_device_channel *const channel)
{
	int ret;
	uint8_t count = 0;

	while (!stp_set_channel_inuse(channel)) {
		if (count == 10) {
			STP_DRV_LOG_ERR("Max number of attempt reached when trying to set channel %d inuse", channel->channel);
			return;
		}

		ret = wait_event_interruptible_timeout(channel->inuse_q, channel->inuse == false, msecs_to_jiffies(STP_WAIT_CHANNEL_TIMEOUT_MS));
		if (ret == 0) {
			STP_DRV_LOG_ERR("Long idle time while setting c%d to be in use, perhaps client did not close FD", channel->channel);
			return;
		}
		else if (ret == -ERESTARTSYS) {
			STP_DRV_LOG_ERR("Interrupted by the system and returning. Channel %d", channel->channel);
			return;
		}

		count++;
	}
	trace_stp_channel_set_inuse(channel->channel, count);
}

void stp_dump_channel_state(void)
{
	uint32_t synced = 0;

	stp_controller_get_attribute(STP_ATTRIB_SYNCED, &synced);
	pr_err("[STPDump] STP channel dump before SPI owner switch (synced=%u):\n", synced);

	for (uint8_t i = 0; i < STP_DEV_CHANNEL_COUNT; i++) {
		struct stp_device_channel *dc = _stp_device ?
						_stp_device->channels[i] : NULL;
		uint32_t rx_filled = 0, tx_data = 0, tx_avail = 0;
		uint32_t dev_conn = 0, ctrl_conn = 0, valid = 0;

		if (!dc)
			continue;

		stp_controller_get_channel_attribute(i, STP_ATTRIB_DEVICE_CONNECTED,
						     &dev_conn);
		stp_controller_get_channel_attribute(i, STP_ATTRIB_CONTROLLER_CONNECTED,
						     &ctrl_conn);
		stp_controller_get_channel_attribute(i, STP_ATTRIB_VALID_SESSION,
						     &valid);

		/* Only query pipeline state if pipelines are initialized */
		if (dev_conn && ctrl_conn) {
			stp_controller_get_channel_attribute(i, STP_RX_FILLED,
							     &rx_filled);
			stp_controller_get_channel_attribute(i, STP_TX_DATA,
							     &tx_data);
			stp_controller_get_channel_attribute(i, STP_TX_AVAILABLE,
							     &tx_avail);
		}

		/* Only log channels that have some activity or state */
		if (!dev_conn && !ctrl_conn && !dc->inuse)
			continue;

		pr_err("[STPDump] ch%02u: session=%u ctrl=%u dev=%u | rx_filled=%u tx_queued=%u tx_avail=%u | opened=%u closing=%u poll_waiters=%u reading=%u\n",
		       i, valid, ctrl_conn, dev_conn,
		       rx_filled, tx_data, tx_avail,
		       dc->inuse,
		       dc->closing,
		       waitqueue_active(&dc->poll_event_q),
		       mutex_is_locked(&dc->rx_lock));
	}
}

static ssize_t stp_stats_show(struct device *dev, struct device_attribute *attr,
			      char *buf)
{
	ssize_t rval;
	int channel;
	struct stp_device_channel *c;

	channel = MINOR(dev->devt);
	c = _stp_device->channels[channel];
	rval = scnprintf(buf, PAGE_SIZE,
			 "channel: %d\n"
			 "inuse: %d\n"
			 "read transactions: %llu\n"
			 "read bytes: %llu\n"
			 "read usecs: %lld\n"
			 "write transactions: %llu\n"
			 "write bytes: %llu\n"
			 "write usecs: %lld\n",
			 channel, c->inuse, c->read_stats.transactions,
			 c->read_stats.total_bytes, ktime_to_us(c->read_stats.total_usecs),
			 c->write_stats.transactions,
			 c->write_stats.total_bytes,
			 ktime_to_us(c->write_stats.total_usecs));
	return rval;
}

static DEVICE_ATTR_RO(stp_stats);

int stp_channel_close(struct stp_device_channel *channel)
{
	if (!channel) {
		STP_DRV_LOG_ERR("STP channel is invalid");
		return -EINVAL;
	}

	// Prevent double releasing the channel as kfree operation
	// can cause kernel panic
	mutex_lock(&channel->release_lock);

	if (!stp_check_channel_inuse(channel)) {
		STP_DRV_LOG_INFO("c%d has been released already. Returning to prevent double release",
						channel->channel);
		goto exit;
	}

// Mark the channel as closing to prevent new operations
  channel->closing = true;

	complete_all(&channel->write_done);
	complete_all(&channel->read_done);
	complete_all(&channel->fsync_done);
	complete_all(&channel->open_done);

	stp_controller_close(channel->channel);

	kfree(channel->rx_buffer);
	channel->rx_buffer = NULL;
	kfree(channel->tx_buffer);
	channel->tx_buffer = NULL;
	kfree(channel->read_buffer);
	channel->read_buffer = NULL;
	kfree(channel->write_buffer);
	channel->write_buffer = NULL;

	stp_unset_channel_inuse(channel);

exit:
	mutex_unlock(&channel->release_lock);
	return 0;
}
EXPORT_SYMBOL(stp_channel_close);

static int stp_device_release(struct inode *inode, struct file *filp)
{
	unsigned int minor;
	struct stp_device_channel *channel;

	if (!_stp_device) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	if (!filp || !inode) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	minor = iminor(inode);
	if (minor >= STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", minor);
		return -EINVAL;
	}

	channel = _stp_device->channels[minor];
	if (!channel) {
		STP_DRV_LOG_ERR("c%d not initialized", minor);
		return -ENODEV;
	}

	WARN_ON(channel->channel != minor);

	return stp_channel_close(channel);
}

struct stp_device_channel* stp_channel_open(int channel_num, bool nonblock)
{
	int rval;
	int controller_rval;
	struct stp_device_channel *channel;

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_INFO("internal data not initialized. probe or remove operation in progress");
		return ERR_PTR(-EINVAL);
	}

	if (stp_get_spi_busy()) {
		return ERR_PTR(-EPIPE);
	}

	channel = _stp_device->channels[channel_num];
	if (!channel) {
		STP_DRV_LOG_ERR("c%d not initialized", channel_num);
		return ERR_PTR(-ENODEV);
	}

	if (!stp_set_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d already in use", channel_num);
		rval = -EPERM;
		goto exit;
	}

	reinit_completion(&channel->write_done);
	reinit_completion(&channel->read_done);
	reinit_completion(&channel->fsync_done);
	reinit_completion(&channel->open_done);

// Reset the closing flag when reopening the channel
  channel->closing = false;

	channel->tx_buffer = kzalloc(channel->tx_buffer_len, GFP_KERNEL);
	if (IS_ERR(channel->tx_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for tx_buffer", channel_num);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->rx_buffer = kzalloc(channel->rx_buffer_len, GFP_KERNEL);
	if (IS_ERR(channel->rx_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for rx_buffer", channel_num);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->read_buffer = kzalloc(STP_XFER_BUFFER_SIZE_BYTES, GFP_KERNEL);
	if (IS_ERR(channel->read_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for read_buffer", channel_num);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->write_buffer = kzalloc(STP_XFER_BUFFER_SIZE_BYTES, GFP_KERNEL);
	if (IS_ERR(channel->write_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for write_buffer", channel_num);
		rval = -ENOMEM;
		goto exit_error;
	}

	if (nonblock) {
		trace_stp_channel_device_nonblocking_open(channel_num);
		controller_rval = stp_controller_open(
			channel_num, channel->priority, channel->rx_buffer,
			channel->rx_buffer_len, channel->tx_buffer,
			channel->tx_buffer_len);
	} else {
		trace_stp_channel_device_blocking_open(channel_num);
		controller_rval = stp_controller_open_blocking(
			channel_num, channel->priority, channel->rx_buffer,
			channel->rx_buffer_len, channel->tx_buffer,
			channel->tx_buffer_len);
	}
	rval = STP_ERR_VAL(controller_rval);
	trace_stp_channel_controller_open(channel_num, controller_rval, rval);
	if (STP_IS_ERR(rval)) {
		if (controller_rval != STP_ERROR_NOT_SYNCED)
			STP_DRV_LOG_ERR_RATE_LIMIT("c%d controller open error: %s %s",
				channel_num,
				get_stp_error_str(controller_rval),
				nonblock ? "in non-blocking calll" : "in blocking call");
		goto exit_error;
	}

	channel->nonblock = nonblock;

	trace_stp_channel_device_open_done(channel_num);
	return channel;

exit_error:
	stp_channel_close(channel);

exit:
	return ERR_PTR(rval);
}
EXPORT_SYMBOL(stp_channel_open);

static int stp_device_open(struct inode *inode, struct file *filp)
{
	unsigned int minor;
	struct stp_device_channel *channel;

	if (!filp || !inode) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	minor = iminor(inode);
	if (minor >= STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", minor);
		return -EINVAL;
	}

	channel = stp_channel_open(minor, !!(filp->f_flags & O_NONBLOCK));

	if (IS_ERR(channel))
		return PTR_ERR(channel);

	filp->private_data = channel;

	return 0;
}

bool stp_check_stale_channel(struct stp_device_channel *channel)
{
	int i;

	if (channel == NULL)
		return true;

	for (i = 0; i < STP_DEV_CHANNEL_COUNT; i++)
		if (channel == _stp_device->channels[i])
			return false;

	return true;
}
EXPORT_SYMBOL(stp_check_stale_channel);

int stp_channel_read(struct stp_device_channel *channel, char *buf,
		     size_t count, bool to_user)
{
	int rval = 0;
	int controller_rval = 0;
	int pending_count;
	char *p_buf;
	unsigned int len;
	int read_count;
	int missing;
	ktime_t after;
	ktime_t before;

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_INFO("internal data not initialized. probe or remove operation in progress");
		return -EINVAL;
	}

	if (stp_get_spi_busy()) {
		return -EPIPE;
	}

	if (stp_check_stale_channel(channel)) {
		STP_DRV_LOG_ERR("Stale channel %p", channel);
		return -EINVAL;
	}

	mutex_lock(&channel->rx_lock);

	if (count == 0) {
		STP_DRV_LOG_ERR("c%d no data to send", channel->channel);
		rval = -EINVAL;
		goto exit_error;
	}

	// If the channel is not in use, we are reading without opening somehow
	if (!stp_check_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d device not open", channel->channel);
		rval = -EPIPE;
		goto exit_error;
	}

	pending_count = count;
	p_buf = buf;

	before = ktime_get();

	if (channel->nonblock) {
		controller_rval = stp_controller_read_nb(channel->channel,
								channel->read_buffer,
								pending_count, &read_count);
		rval = STP_ERR_VAL(controller_rval);
		trace_stp_channel_read(channel->channel, 0, pending_count, count, read_count, controller_rval, rval);
		if (STP_IS_ERR(rval)) {
			if (controller_rval == STP_ERROR_SERVICE_INTERRUPTION)
				STP_DRV_LOG_INFO("c%d controller error: %s in non-blocking call",
						channel->channel, get_stp_error_str(controller_rval));
			else if (rval != -ERESTARTSYS && controller_rval != STP_ERROR_INVALID_SESSION)
				STP_DRV_LOG_ERR("c%d controller error: %s in non-blocking call",
						channel->channel, get_stp_error_str(controller_rval));
			goto exit_error;
		}

		if (read_count != 0) {
			if (to_user) {
				missing = copy_to_user(p_buf, channel->read_buffer, read_count);
				if (missing != 0) {
					STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
							channel->channel, missing);
					rval = -EFAULT;
					goto exit_error;
				}
			} else {
				memcpy(p_buf, channel->read_buffer, read_count);
			}
		}

		pending_count -= read_count;
	}
	else {
		while (pending_count > 0) {
			len = STP_XFER_BUFFER_SIZE_BYTES;
			if (len > (unsigned int)pending_count)
				len = pending_count;

			controller_rval = stp_controller_read(channel->channel,
								channel->read_buffer,
								len, &read_count);
			rval = STP_ERR_VAL(controller_rval);
			trace_stp_channel_read(channel->channel, 1, pending_count, len, read_count, controller_rval, rval);
			if (STP_IS_ERR(rval)) {
				if (controller_rval == STP_ERROR_SERVICE_INTERRUPTION)
					STP_DRV_LOG_INFO("c%d controller error: %s in blocking call",
						channel->channel, get_stp_error_str(controller_rval));
				else if(rval != -ERESTARTSYS && controller_rval != STP_ERROR_INVALID_SESSION)
					STP_DRV_LOG_ERR("c%d controller error: %s in blocking call",
						channel->channel, get_stp_error_str(controller_rval));
				goto exit_error;
			}

			if (read_count != len) {
				STP_DRV_LOG_ERR(
					"c%d len error: expected `%d` received `%d`",
					channel->channel, len, read_count);
				rval = -EFAULT;
				goto exit_error;
			}

			if (to_user) {
				missing = copy_to_user(p_buf, channel->read_buffer, len);
				if (missing != 0) {
					STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
							channel->channel, missing);
					rval = -EFAULT;
					goto exit_error;
				}
			} else {
				memcpy(p_buf, channel->read_buffer, len);
			}

			p_buf += len;
			pending_count -= len;
		}
	}

	rval = count - pending_count;

	after = ktime_get();
	channel->read_stats.transactions++;
	channel->read_stats.total_bytes += rval;
	channel->read_stats.total_usecs = ktime_add(channel->read_stats.total_usecs, ktime_sub(after, before));

exit_error:
	mutex_unlock(&channel->rx_lock);

	trace_stp_channel_read_exit(channel->channel, rval);
	return rval;
}
EXPORT_SYMBOL(stp_channel_read);

static ssize_t stp_device_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *f_pos)
{
	struct stp_device_channel *channel;

	if (!filp || !buf || !filp->private_data) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	channel = filp->private_data;

	return stp_channel_read(channel, buf, count, true);
}

int stp_channel_write(struct stp_device_channel *channel,
		      const char *buf, size_t count, bool from_user)
{
	int rval = 0;
	int controller_rval = 0;
	const char *p_buf;
	int pending_count;
	unsigned int len;
	int send_count;
	int missing;
	ktime_t after;
	ktime_t before;

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_INFO("internal data not initialized. probe or remove operation in progress");
		return -EINVAL;
	}

	if (stp_get_spi_busy())
		return -EPIPE;

	if (stp_check_stale_channel(channel)) {
		STP_DRV_LOG_ERR("Stale channel %p", channel);
		return -EINVAL;
	}

	mutex_lock(&channel->tx_lock);

	if (count == 0) {
		STP_DRV_LOG_ERR("c%d no data to send", channel->channel);
		rval = -EINVAL;
		goto exit_error;
	}

	// If the channel is not in use, we are writing without opening somehow
	if (!stp_check_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d device not open", channel->channel);
		rval = -EPIPE;
		goto exit_error;
	}

	p_buf = buf;
	pending_count = count;

	before = ktime_get();

	if (channel->nonblock) {
		len = STP_XFER_BUFFER_SIZE_BYTES;

		if (len > pending_count)
			len = (unsigned int)pending_count;

		if (from_user) {
			missing = copy_from_user(channel->write_buffer, p_buf, len);
			if (missing != 0) {
				STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
						channel->channel, missing);
				rval = -EFAULT;
				goto exit_error;
			}
		} else {
			memcpy(channel->write_buffer, p_buf, len);
		}

		controller_rval = stp_controller_write_nb(channel->channel,
							channel->write_buffer,
							len, &send_count);

		rval = STP_ERR_VAL(controller_rval);
		trace_stp_channel_write(channel->channel, 0, pending_count, len, send_count, controller_rval, rval);
		if (STP_IS_ERR(rval)) {
			if (controller_rval == STP_ERROR_SERVICE_INTERRUPTION)
				STP_DRV_LOG_INFO("c%d failed to write all data: %s in non-blocking call",
						channel->channel, get_stp_error_str(controller_rval));
			else if(rval != -ERESTARTSYS && controller_rval != STP_ERROR_INVALID_SESSION)
				STP_DRV_LOG_ERR("c%d failed to write all data: %s in non-blocking call",
						channel->channel, get_stp_error_str(controller_rval));
			goto exit_error;
		}

		pending_count -= send_count;
	} else {
		while (pending_count > 0) {
			len = STP_XFER_BUFFER_SIZE_BYTES;

			if (len > pending_count)
				len = (unsigned int)pending_count;

			if (from_user) {
				missing = copy_from_user(channel->write_buffer, p_buf, len);

				if (missing != 0) {
					STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
							channel->channel, missing);
					rval = -EFAULT;
					goto exit_error;
				}
			} else {
				memcpy(channel->write_buffer, p_buf, len);
			}

			controller_rval = stp_controller_write(channel->channel,
								channel->write_buffer,
								len, &send_count);

			rval = STP_ERR_VAL(controller_rval);
			trace_stp_channel_write(channel->channel, 0, pending_count, len, send_count, controller_rval, rval);
			if (STP_IS_ERR(rval)) {
				if (controller_rval == STP_ERROR_SERVICE_INTERRUPTION)
					STP_DRV_LOG_INFO("c%d failed to write all data: %s in blocking call",
							channel->channel, get_stp_error_str(controller_rval));
				else if(rval != -ERESTARTSYS && controller_rval != STP_ERROR_INVALID_SESSION)
					STP_DRV_LOG_ERR("c%d failed to write all data: %s in blocking call",
							channel->channel, get_stp_error_str(controller_rval));
				goto exit_error;
			}

			if (send_count != len) {
				STP_DRV_LOG_ERR(
					"c%d write byte count error: expected `%d` sent `%d`\n",
					channel->channel, len, send_count);
				rval = -EFAULT;
				goto exit_error;
			}

			p_buf += len;
			pending_count -= len;
		}
	}

	rval = count - pending_count;

	after = ktime_get();

	channel->write_stats.transactions++;
	channel->write_stats.total_bytes += rval;
	channel->write_stats.total_usecs = ktime_add(channel->write_stats.total_usecs, ktime_sub(after, before));

exit_error:
	mutex_unlock(&channel->tx_lock);

	trace_stp_channel_write_exit(channel->channel, rval);

	return rval;
}
EXPORT_SYMBOL(stp_channel_write);

static ssize_t stp_device_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	struct stp_device_channel *channel;

	if (!filp || !buf || !filp->private_data) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	channel = filp->private_data;

	return stp_channel_write(channel, buf, count, true);
}

static int stp_device_fsync(struct file *filp, loff_t start, loff_t end,
			    int datasync)
{
	int rval = 0;
	int controller_rval = 0;
	struct stp_device_channel *channel;

	if (!filp || !filp->private_data) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_INFO("internal data not initialized. probe or remove operation in progress");
		return -EINVAL;
	}

	if (stp_get_spi_busy()) {
		return -EPIPE;
	}

	channel = filp->private_data;
	if (stp_check_stale_channel(channel)) {
		STP_DRV_LOG_ERR("Stale channel %p", channel);
		return -EINVAL;
	}

	mutex_lock(&channel->tx_lock);

	// If the channel is not in use, we are writing without opening somehow
	if (!stp_check_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d device not open", channel->channel);
		rval = -EPIPE;
		goto exit_error;
	}

	controller_rval = stp_controller_fsync(channel->channel);
	rval = STP_ERR_VAL(controller_rval);
	if (STP_IS_ERR(rval)) {
		if(rval != -ERESTARTSYS && controller_rval != STP_ERROR_INVALID_SESSION)
			STP_DRV_LOG_ERR("fsync error: %s",
							get_stp_error_str(controller_rval));
	}

exit_error:
	mutex_unlock(&channel->tx_lock);

	return rval;
}

int stp_channel_connected(uint32_t channel, uint32_t *connected)
{
	int ret;

	ret = stp_controller_get_channel_attribute(channel,
					STP_ATTRIB_DEVICE_CONNECTED, connected);
	ret = STP_ERR_VAL(ret);
	if (ret)
		return ret;

	if (*connected == 0)
		return 0;

	ret = stp_controller_get_channel_attribute(channel,
				    STP_ATTRIB_CONTROLLER_CONNECTED, connected);
	ret = STP_ERR_VAL(ret);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL(stp_channel_connected);

int stp_channel_rx_filled(uint32_t channel, uint32_t *rx_data_avail)
{
	int ret;

	ret = stp_controller_get_channel_attribute(channel, STP_RX_FILLED,
						   rx_data_avail);
	ret = STP_ERR_VAL(ret);
	STP_ASSERT(!STP_IS_ERR(ret), "Error getting attribute STP_RX_FILLED");

	return ret;
}
EXPORT_SYMBOL(stp_channel_rx_filled);

int stp_protocol_synced(uint32_t *synced)
{
	int ret;

	ret = stp_controller_get_attribute(STP_ATTRIB_SYNCED, synced);
	ret = STP_ERR_VAL(ret);

	return ret;
}
EXPORT_SYMBOL(stp_protocol_synced);

static __poll_t stp_get_poll_events(uint32_t channel, __poll_t requested_events)
{
	__poll_t events = 0;
	uint32_t ret = 0;
	uint32_t controller_connected = 0;
	uint32_t device_connected = 0;
	uint32_t tx_pipeline_has_space = 0;
	uint32_t rx_data_available = 0;


	ret = STP_ERR_VAL(stp_controller_get_channel_attribute(channel, STP_ATTRIB_DEVICE_CONNECTED, &device_connected));
	STP_ASSERT(!STP_IS_ERR(ret), "Error getting attribute STP_ATTRIB_DEVICE_CONNECTED");

	if (!device_connected)
		events |= POLLHUP;

	ret = STP_ERR_VAL(stp_controller_get_channel_attribute(channel, STP_ATTRIB_CONTROLLER_CONNECTED, &controller_connected));
	STP_ASSERT(!STP_IS_ERR(ret), "Error getting attribute STP_ATTRIB_CONTROLLER_CONNECTED");

	if (!controller_connected)
		events |= POLLHUP;

	if (requested_events & POLLOUT) {
		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(channel, STP_TX_AVAILABLE, &tx_pipeline_has_space));
		STP_ASSERT(!STP_IS_ERR(ret), "Error getting attribute STP_TX_AVAILABLE");
	}

	if (tx_pipeline_has_space && device_connected)
		events |= (POLLOUT | POLLWRNORM);

	if (requested_events & POLLIN) {
		ret = STP_ERR_VAL(stp_controller_get_channel_attribute(channel, STP_RX_FILLED, &rx_data_available));
		STP_ASSERT(!STP_IS_ERR(ret), "Error getting attribute STP_RX_FILLED");
	}

	if (rx_data_available)
		events |= (POLLIN | POLLRDNORM);

	if (!_stp_device->device_ready || stp_get_spi_busy())
		events |= POLLERR;

	return events;
}

unsigned int stp_device_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t events;
	__poll_t requested_events;
	struct stp_device_channel *channel;

	if (!filp || !filp->private_data) {
		STP_DRV_LOG_ERR("invalid parameters");
		return POLLERR;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_INFO("internal data not initialized. probe or remove operation in progress");
		return POLLERR;
	}

	channel = filp->private_data;
	if (stp_check_stale_channel(channel)) {
		STP_DRV_LOG_ERR("Stale channel %p", channel);
		return -EINVAL;
	}

	requested_events = poll_requested_events(wait);
	trace_stp_channel_poll_enter(channel->channel, requested_events);
	events = stp_get_poll_events(channel->channel, requested_events);

	if (!events) {
		poll_wait(filp, &channel->poll_event_q, wait);
		events = stp_get_poll_events(channel->channel, requested_events);
	}

	trace_stp_channel_poll_exit(channel->channel, events);

	return events;
}

static const struct file_operations stp_device_fops = {
	.owner = THIS_MODULE,
	.open = stp_device_open,
	.release = stp_device_release,
	.read = stp_device_read,
	.write = stp_device_write,
	.fsync = stp_device_fsync,
	.poll = stp_device_poll,
	.llseek = no_llseek,
};

bool stp_get_spi_busy()
{
	bool spi_busy_copy;
	mutex_lock(&stp_spi_busy_lock);
	spi_busy_copy = spi_busy;
	mutex_unlock(&stp_spi_busy_lock);

	return spi_busy_copy;
}
EXPORT_SYMBOL(stp_get_spi_busy);

void stp_set_spi_busy(bool busy)
{
	mutex_lock(&stp_spi_busy_lock);
	spi_busy = busy;
	mutex_unlock(&stp_spi_busy_lock);
}

int stp_create_channel(struct stp_channel_data *const data)
{
	struct stp_device_channel *working_channel;

	if (!data) {
		STP_DRV_LOG_ERR("bad channel input");
		return -EINVAL;
	}

	if (data->channel >= STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d out of bounds", data->channel);
		return -EINVAL;
	}

	if (_stp_device->channels[data->channel]) {
		STP_DRV_LOG_ERR("c%d already exists", data->channel);
		return -EEXIST;
	}

	working_channel = devm_kzalloc(_stp_device->parent_dev,
				       sizeof(*working_channel), GFP_KERNEL);
	if (IS_ERR(working_channel)) {
		STP_DRV_LOG_ERR("c%d cannot allocate", data->channel);
		return -ENOMEM;
	}

	working_channel->devt = MKDEV(_stp_device->major, data->channel);
	working_channel->dev =
		device_create(_stp_device->device_class,
			      _stp_device->parent_dev, working_channel->devt,
			      NULL, "stp%d", data->channel);

	if (IS_ERR(working_channel->dev)) {
		STP_DRV_LOG_ERR("c%d class create error", data->channel);
		return PTR_ERR(working_channel->dev);
	}

	init_completion(&working_channel->write_done);
	init_completion(&working_channel->read_done);
	init_completion(&working_channel->fsync_done);
	init_completion(&working_channel->open_done);

	init_waitqueue_head(&working_channel->poll_event_q);
	init_waitqueue_head(&working_channel->inuse_q);

	working_channel->rx_buffer_len = data->rx_len_bytes;
	working_channel->tx_buffer_len = data->tx_len_bytes;
	working_channel->priority = data->priority;
	working_channel->channel = data->channel;
  working_channel->closing = false;

	mutex_init(&working_channel->inuse_lock);
	mutex_init(&working_channel->rx_lock);
	mutex_init(&working_channel->tx_lock);
	mutex_init(&working_channel->release_lock);

	/*
	 * compiler barrier to ensure all instructions for the working_channel
	 * to be done before populate.
	 */
	barrier();
	_stp_device->channels[data->channel] = working_channel;

	working_channel->inuse = false;

	device_create_file(working_channel->dev, &dev_attr_stp_stats);

	return 0;
}

static int stp_interrupt_channel(uint8_t channel)
{
	struct stp_device_channel *working_channel;

	if (channel >= STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", channel);
		return -EINVAL;
	}

	if (!_stp_device->channels[channel]) {
		STP_DRV_LOG_ERR("c%d does not exist", channel);
		return -ENODEV;
	}

	working_channel = _stp_device->channels[channel];

	trace_stp_channel_interrupt(channel);
	/* In practice no further I/O is possible.  All paths invoking an
	 * interruption lead to channel closing.
	 *
	 * Set the "closing" flag here to inform completion waiters
	 * that there is no point in waiting.*/
	working_channel->closing = true;
	wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
	complete_all(&working_channel->write_done);
	complete_all(&working_channel->read_done);
	complete_all(&working_channel->fsync_done);
	complete_all(&working_channel->open_done);

	return 0;
}

int stp_release_channel(uint8_t channel)
{
	int ret;
	struct stp_device_channel *working_channel;

	ret = stp_interrupt_channel(channel);
	if (ret != 0)
		return ret;

	working_channel = _stp_device->channels[channel];
	stp_wait_and_set_channel_inuse(working_channel);

	return 0;
}

int stp_remove_channel(uint8_t channel)
{
	struct stp_device_channel *working_channel;

	if (channel >= STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", channel);
		return -EINVAL;
	}

	if (!_stp_device->channels[channel]) {
		STP_DRV_LOG_ERR("c%d does not exist", channel);
		return -ENODEV;
	}

	working_channel = _stp_device->channels[channel];

	device_destroy(_stp_device->device_class, working_channel->devt);

	devm_kfree(_stp_device->parent_dev, working_channel);
	_stp_device->channels[channel] = NULL;

	return 0;
}

int stp_interrupt_all_channels(void)
{
	int ret;
	for (int i = 0; i < STP_DEV_CHANNEL_COUNT; i++) {
		if (_stp_device->channels[i])
			ret = stp_interrupt_channel(i);
		if (ret)
			STP_DRV_LOG_ERR("Failed to interrupt channel %d", i);
	}

	return 0;
}

// Stateful call, will initialize the device and cache the
// metadata associated with channel creation.
int stp_create_device(struct device *dev)
{
	int rval = 0;
	int major;

	if (_stp_device) {
		STP_DRV_LOG_ERR("already intialized");
		return -EEXIST;
	}

	_stp_device = devm_kzalloc(dev, sizeof(*_stp_device), GFP_KERNEL);
	if (IS_ERR(_stp_device))
		return -ENOMEM;

	major = register_chrdev(0, STP_DEVICE_NAME, &stp_device_fops);

	if (major < 0) {
		STP_DRV_LOG_ERR("register_chrdev error `%d`", major);
		rval = major;
		goto exit_error;
	}

	_stp_device->major = major;

	_stp_device->device_class = class_create(THIS_MODULE, STP_DEVICE_NAME);
	if (IS_ERR(_stp_device->device_class)) {
		STP_DRV_LOG_ERR("class create error");
		rval = PTR_ERR(_stp_device->device_class);
		goto exit_class_error;
	}

	_stp_device->parent_dev = dev;

	stp_set_spi_busy(false);

	mutex_init(&_stp_device->device_ready_lock);
	stp_set_device_ready(true);

	STP_DRV_LOG_ERR("Device created");
	return 0;

exit_class_error:
	unregister_chrdev(major, STP_DEVICE_NAME);

// fallthrough
exit_error:
	devm_kfree(dev, _stp_device);
	_stp_device = NULL;

	return rval;
}

int stp_remove_device(struct device *dev)
{
	unsigned int i;

	stp_set_device_ready(false);

	for (i = 0; i < STP_DEV_CHANNEL_COUNT; i++) {
		if (_stp_device->channels[i])
			stp_release_channel(i);
	}

	for (i = 0; i < STP_DEV_CHANNEL_COUNT; i++) {
		if (_stp_device->channels[i])
			stp_remove_channel(i);
	}

	class_destroy(_stp_device->device_class);
	unregister_chrdev(_stp_device->major, STP_DEVICE_NAME);

	devm_kfree(dev, _stp_device);
	_stp_device = NULL;

	return 0;
}
