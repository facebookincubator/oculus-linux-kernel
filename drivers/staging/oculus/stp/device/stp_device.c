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
#include <stp/controller/stp_controller.h>

#define STP_DEVICE_NAME "stp"
#define STP_DEV_CHANNEL_COUNT 32
#define STP_XFER_BUFFER_SIZE_BYTES (1024 * 5)
#define STP_WAIT_CHANNEL_TIMEOUT_MS	2000

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

	bool inuse;

	struct stp_device_stats read_stats;
	struct stp_device_stats write_stats;
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

static bool stp_get_device_ready(void)
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
	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("c%d not initialized", channel);
		return false;
	}

	if (channel > STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d out of bounds", channel);
		return false;
	}

	if (!_stp_device->channels[channel])
		return false;

	return true;
}

void stp_channel_signal_write(uint8_t channel)
{
	if (validate_channel(channel)) {
		wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
		complete(&_stp_device->channels[channel]->write_done);
	}
}

int stp_channel_wait_write(uint8_t channel)
{
	if (validate_channel(channel))
		return wait_for_completion_interruptible(
			&_stp_device->channels[channel]->write_done);

	return -ENODEV;
}

void stp_channel_signal_read(uint8_t channel)
{
	if (validate_channel(channel)) {
		wake_up_interruptible(&_stp_device->channels[channel]->poll_event_q);
		complete(&_stp_device->channels[channel]->read_done);
	}
}

int stp_channel_wait_read(uint8_t channel)
{
	if (validate_channel(channel))
		return wait_for_completion_interruptible(
			&_stp_device->channels[channel]->read_done);

	return -ENODEV;
}

void stp_channel_signal_fsync(uint8_t channel)
{
	if (validate_channel(channel))
		complete(&_stp_device->channels[channel]->fsync_done);
}

int stp_channel_wait_fsync(uint8_t channel)
{
	if (validate_channel(channel))
		return wait_for_completion_interruptible(
			&_stp_device->channels[channel]->fsync_done);

	return -ENODEV;
}

void stp_channel_reset_fsync(uint8_t channel)
{
	if (validate_channel(channel))
		reinit_completion(&_stp_device->channels[channel]->fsync_done);
}

int stp_channel_wait_open(uint8_t channel)
{
	if (validate_channel(channel))
		return wait_for_completion_interruptible(
			&_stp_device->channels[channel]->open_done);

	return -ENODEV;
}

void stp_channel_signal_open(uint8_t channel)
{
	if (validate_channel(channel)) {
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
	channel->inuse = false;
	mutex_unlock(&channel->inuse_lock);
	wake_up_interruptible(&channel->inuse_q);
}

static void stp_wait_and_set_channel_inuse(struct stp_device_channel *const channel)
{
	int ret;

	while (!stp_set_channel_inuse(channel)) {
		ret = wait_event_interruptible_timeout(channel->inuse_q, channel->inuse == false, msecs_to_jiffies(STP_WAIT_CHANNEL_TIMEOUT_MS));
		if (ret == 0) {
			STP_DRV_LOG_ERR("Long idle time while setting c%d to be in use, perhaps client did not close FD", channel->channel);
			return;
		}
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
			 c->read_stats.total_bytes, ktime_to_ns(c->read_stats.total_usecs),
			 c->write_stats.transactions,
			 c->write_stats.total_bytes,
			 ktime_to_ns(c->write_stats.total_usecs));
	return rval;
}

static DEVICE_ATTR_RO(stp_stats);

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
	if (minor > STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", minor);
		return -EINVAL;
	}

	channel = _stp_device->channels[minor];
	if (!channel) {
		STP_DRV_LOG_ERR("c%d not initialized", minor);
		return -ENODEV;
	}

	complete_all(&channel->write_done);
	complete_all(&channel->read_done);
	complete_all(&channel->fsync_done);
	complete_all(&channel->open_done);

	stp_controller_close(minor);

	kfree(channel->rx_buffer);
	channel->rx_buffer = NULL;
	kfree(channel->tx_buffer);
	channel->tx_buffer = NULL;
	kfree(channel->read_buffer);
	channel->read_buffer = NULL;
	kfree(channel->write_buffer);
	channel->write_buffer = NULL;

	stp_unset_channel_inuse(channel);

	return 0;
}

static int stp_device_open(struct inode *inode, struct file *filp)
{
	unsigned int minor;
	int rval;
	struct stp_device_channel *channel;

	if (!filp || !inode) {
		STP_DRV_LOG_ERR("invalid parameters");
		return -EINVAL;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("internal data not initialized");
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
		rval = -ENODEV;
		goto exit_error;
	}

	if (!stp_set_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d already in use", minor);
		rval = -EPERM;
		goto exit_error;
	}

	reinit_completion(&channel->write_done);
	reinit_completion(&channel->read_done);
	reinit_completion(&channel->fsync_done);
	reinit_completion(&channel->open_done);

	channel->tx_buffer = kzalloc(channel->tx_buffer_len, GFP_KERNEL);
	if (IS_ERR(channel->tx_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for tx_buffer", minor);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->rx_buffer = kzalloc(channel->rx_buffer_len, GFP_KERNEL);
	if (IS_ERR(channel->rx_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for rx_buffer", minor);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->read_buffer = kzalloc(STP_XFER_BUFFER_SIZE_BYTES, GFP_KERNEL);
	if (IS_ERR(channel->read_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for read_buffer", minor);
		rval = -ENOMEM;
		goto exit_error;
	}

	channel->write_buffer = kzalloc(STP_XFER_BUFFER_SIZE_BYTES, GFP_KERNEL);
	if (IS_ERR(channel->read_buffer)) {
		STP_DRV_LOG_ERR("c%d no memory for write_buffer", minor);
		rval = -ENOMEM;
		goto exit_error;
	}

	if (filp->f_flags & O_NONBLOCK) {
		rval = STP_ERR_VAL(stp_controller_open(
			minor, channel->priority, channel->rx_buffer,
			channel->rx_buffer_len, channel->tx_buffer,
			channel->tx_buffer_len));
	} else {
		rval = STP_ERR_VAL(stp_controller_open_blocking(
			minor, channel->priority, channel->rx_buffer,
			channel->rx_buffer_len, channel->tx_buffer,
			channel->tx_buffer_len));
	}
	if (STP_IS_ERR(rval)) {
		STP_DRV_LOG_ERR_RATE_LIMIT("c%d controller open error", minor);
		goto exit_error;
	}

	filp->private_data = channel;

	return 0;

exit_error:
	stp_device_release(inode, filp);

	return rval;
}

static bool is_staled_channel(struct stp_device_channel *channel)
{
	int i;

	if (channel == NULL)
		return true;

	for (i = 0; i < STP_DEV_CHANNEL_COUNT; i++)
		if (channel == _stp_device->channels[i])
			return false;

	return true;
}

static ssize_t stp_device_read(struct file *filp, char __user *buf,
			       size_t count, loff_t *f_pos)
{
	int rval = 0;
	struct stp_device_channel *channel;
	int pending_count;
	char *p_buf;
	unsigned int len;
	int read_count;
	int missing;
	ktime_t after;
	ktime_t before;

	if (!filp || !buf || !filp->private_data) {
		STP_DRV_LOG_ERR("bad input");
		return -EINVAL;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	channel = filp->private_data;
	if (is_staled_channel(channel)) {
		STP_DRV_LOG_ERR(">>>>>>>> Staled channel %p *******", channel);
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

	if (filp->f_flags & O_NONBLOCK) {
		rval = STP_ERR_VAL(stp_controller_read_nb(channel->channel,
								channel->read_buffer,
								pending_count, &read_count));
		if (STP_IS_ERR(rval)) {
			if (rval != -ERESTARTSYS)
				STP_DRV_LOG_ERR("c%d controller error `%d` in non-blocking call",
						channel->channel, rval);
			goto exit_error;
		}

		if (read_count != 0)
		{
			missing = copy_to_user(p_buf, channel->read_buffer, read_count);
			if (missing != 0) {
				STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
						channel->channel, missing);
				rval = -EFAULT;
				goto exit_error;
			}
		}

		pending_count -= read_count;
	}
	else {
		while (pending_count > 0) {
			len = STP_XFER_BUFFER_SIZE_BYTES;
			if (len > (unsigned int)pending_count)
				len = pending_count;

			rval = STP_ERR_VAL(stp_controller_read(channel->channel,
								channel->read_buffer,
								len, &read_count));
			if (STP_IS_ERR(rval)) {
				if(rval != -ERESTARTSYS)
					STP_DRV_LOG_ERR("c%d controller error `%d`",
							channel->channel, rval);
				goto exit_error;
			}

			if (read_count != len) {
				STP_DRV_LOG_ERR(
					"c%d len error: expected `%d` received `%d`",
					channel->channel, len, read_count);
				rval = -EFAULT;
				goto exit_error;
			}

			missing = copy_to_user(p_buf, channel->read_buffer, len);
			if (missing != 0) {
				STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
						channel->channel, missing);
				rval = -EFAULT;
				goto exit_error;
			}

			p_buf += len;
			pending_count -= len;
		}
	}

	rval = count - pending_count;

	after = ktime_get();
	channel->read_stats.transactions++;
	channel->read_stats.total_bytes += rval;
	channel->read_stats.total_usecs = ktime_add(channel->read_stats.total_usecs, ktime_sub(before, after));

exit_error:
	mutex_unlock(&channel->rx_lock);

	return rval;
}

static ssize_t stp_device_write(struct file *filp, const char __user *buf,
				size_t count, loff_t *f_pos)
{
	int rval = 0;
	const char *p_buf;
	struct stp_device_channel *channel;
	int pending_count;
	unsigned int len;
	int send_count;
	int missing;
	ktime_t after;
	ktime_t before;

	if (!filp || !buf || !filp->private_data) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	channel = filp->private_data;

	if (is_staled_channel(channel)) {
		STP_DRV_LOG_ERR(">>>>>>>> Staled channel %p *******", channel);
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

	if (filp->f_flags & O_NONBLOCK) {
		len = STP_XFER_BUFFER_SIZE_BYTES;

		if (len > pending_count)
			len = (unsigned int)pending_count;

		missing = copy_from_user(channel->write_buffer, p_buf, len);
		if (missing != 0) {
			STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
					channel->channel, missing);
			rval = -EFAULT;
			goto exit_error;
		}

		rval = STP_ERR_VAL(stp_controller_write_nb(channel->channel,
							channel->write_buffer,
							len, &send_count));

		if (STP_IS_ERR(rval)) {
				STP_DRV_LOG_ERR("c%d failed to write all data",
						channel->channel);
				goto exit_error;
		}

		pending_count -= send_count;
	}
	else {
		while (pending_count > 0) {
			len = STP_XFER_BUFFER_SIZE_BYTES;

			if (len > pending_count)
				len = (unsigned int)pending_count;

			missing = copy_from_user(channel->write_buffer, p_buf, len);

			if (missing != 0) {
				STP_DRV_LOG_ERR("c%d failed to copy `%d` bytes",
						channel->channel, missing);
				rval = -EFAULT;
				goto exit_error;
			}

			rval = STP_ERR_VAL(stp_controller_write(channel->channel,
								channel->write_buffer,
								len, &send_count));

			if (STP_IS_ERR(rval)) {
				STP_DRV_LOG_ERR("c%d failed to write all data",
						channel->channel);
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
	channel->write_stats.total_usecs = ktime_add(channel->write_stats.total_usecs, ktime_sub(before, after));

exit_error:
	mutex_unlock(&channel->tx_lock);

	return rval;
}

static int stp_device_fsync(struct file *filp, loff_t start, loff_t end,
			    int datasync)
{
	int rval = 0;
	struct stp_device_channel *channel;

	if (!filp || !filp->private_data) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return -EINVAL;
	}

	channel = filp->private_data;
	mutex_lock(&channel->tx_lock);

	// If the channel is not in use, we are writing without opening somehow
	if (!stp_check_channel_inuse(channel)) {
		STP_DRV_LOG_ERR("c%d device not open", channel->channel);
		rval = -EPIPE;
		goto exit_error;
	}

	rval = STP_ERR_VAL(stp_controller_fsync(channel->channel));

exit_error:
	mutex_unlock(&channel->tx_lock);

	return rval;
}

static __poll_t stp_get_poll_events(uint32_t channel, __poll_t requested_events)
{
	__poll_t events = 0;
	uint32_t device_connected = 0;
	uint32_t tx_pipeline_has_space = 0;
	uint32_t rx_data_available = 0;

	if (requested_events & POLLOUT) {
		stp_controller_get_channel_attribute(channel, STP_ATTRIB_DEVICE_CONNECTED, &device_connected);
		stp_controller_get_channel_attribute(channel, STP_TX_AVAILABLE, &tx_pipeline_has_space);
	}

	if (tx_pipeline_has_space && device_connected)
		events |= (POLLOUT | POLLWRNORM);

	if (requested_events & POLLIN)
		stp_controller_get_channel_attribute(channel, STP_RX_FILLED, &rx_data_available);

	if (rx_data_available)
		events |= (POLLIN | POLLRDNORM);

	if (!_stp_device->device_ready)
		events |= POLLERR;

	return events;
}

unsigned int stp_device_poll(struct file *filp, struct poll_table_struct *wait)
{
	__poll_t events;
	__poll_t requested_events;
	struct stp_device_channel *channel;

	if (!filp || !filp->private_data) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return POLLERR;
	}

	if (!stp_get_device_ready()) {
		STP_DRV_LOG_ERR("internal data not initialized");
		return POLLERR;
	}

	channel = filp->private_data;
	requested_events = poll_requested_events(wait);
	events = stp_get_poll_events(channel->channel, requested_events);

	if (!events) {
		poll_wait(filp, &channel->poll_event_q, wait);
		events = stp_get_poll_events(channel->channel, requested_events);
	}

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

	_stp_device->channels[data->channel] = working_channel;
	working_channel->rx_buffer_len = data->rx_len_bytes;
	working_channel->tx_buffer_len = data->tx_len_bytes;
	working_channel->priority = data->priority;
	working_channel->channel = data->channel;

	mutex_init(&working_channel->inuse_lock);
	mutex_init(&working_channel->rx_lock);
	mutex_init(&working_channel->tx_lock);

	working_channel->inuse = false;

	device_create_file(working_channel->dev, &dev_attr_stp_stats);

	return 0;
}

int stp_release_channel(uint8_t channel)
{
	struct stp_device_channel *working_channel;

	if (channel > STP_DEV_CHANNEL_COUNT) {
		STP_DRV_LOG_ERR("c%d index out of bounds", channel);
		return -EINVAL;
	}

	if (!_stp_device->channels[channel]) {
		STP_DRV_LOG_ERR("c%d does not exist", channel);
		return -ENODEV;
	}

	working_channel = _stp_device->channels[channel];
	wake_up_interruptible(&working_channel->poll_event_q);

	complete_all(&working_channel->write_done);
	complete_all(&working_channel->read_done);
	complete_all(&working_channel->fsync_done);
	complete_all(&working_channel->open_done);

	stp_wait_and_set_channel_inuse(working_channel);

	return 0;
}

int stp_remove_channel(uint8_t channel)
{
	struct stp_device_channel *working_channel;

	if (channel > STP_DEV_CHANNEL_COUNT) {
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

	mutex_init(&_stp_device->device_ready_lock);
	stp_set_device_ready(true);

	STP_DRV_LOG_ERR("device created");
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
