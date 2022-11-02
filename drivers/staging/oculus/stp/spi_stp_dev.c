// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/kthread.h>
#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/uaccess.h>

#include <stp_master.h>
#include <spi_stp_dev.h>
#include <stp_debug.h>
#include <stp_master_common.h>
#include <stp_router.h>

#define STP_DEV_MAJOR			0	/* dynamic */
#define N_STP_DEV_MINORS		32

static DECLARE_BITMAP(minors, N_STP_DEV_MINORS);

#define STP_DEV_DEVICE_NAME				"stp0l"

#define SIZE_STP_DEV_BUFFER	(1024 * 5)

#define LINE_SIZE					128

#define DEFAULT_MAX_CLIENTS	2
#define STP_ROUTER_THREAD_PRIORITY	50
#define SIZE_STP_ROUTER_RX_PIPELINE	(1024 * 512)


static int stp_start;

struct stp_user_data {
	/* this is the client id to pass to the stp router */
	/*stp_router handle */
	stp_router_handle stpr_hdl;

	u8 *tx_buffer;
	unsigned int tx_len;

	u8 *rx_buffer;
	unsigned int rx_len;

	/* this is the buffer used exclusively by the router */
	u8 *router_buffer;
	unsigned int router_buffer_len;

	/* the mutex used to synchonize the read requests */
	struct mutex rx_lock;

	/* the mutex used to synchonize the write requests */
	struct mutex tx_lock;

	struct completion read_done;
	struct completion write_done;

	bool inuse;
};

/* STP data internal data */
struct stp_dev {
	struct device *dev;

	struct device *stp_dev;
	dev_t	devt;

	/*
	 * TODO: allocate user_data dinamically on open().
	 * That way, all buffers will be handled by one single alloc
	 */
	struct stp_user_data user_data[DEFAULT_MAX_CLIENTS];

	struct task_struct *router_thread;

	/* the mutex used to synchonize the open/close requests */
	struct mutex oc_lock;

	struct class *dev_class;

	bool open;
};

static struct stp_dev *_stp_dev;

//---------------------------------------------------------

static int spi_stp_dev_map_errors(int stp_error)
{
	int ret = STP_ROUTER_SUCCESS;

	switch (stp_error) {
	case STP_SUCCESS:
		ret = STP_ROUTER_SUCCESS;
		break;
	case STP_ERROR:
		ret = -EAGAIN;
		break;
	case STP_ERROR_SLAVE_NOT_CONNECTED:
		ret = -EAGAIN;
		break;
	case STP_ERROR_MASTER_NOT_CONNECTED:
	case STP_ERROR_NOT_SYNCED:
	case STP_ERROR_MASTER_ALREADY_OPEN:
	case STP_ERROR_MASTER_ALREADY_CLOSED:
		ret = -EBADF;
		break;
	case STP_ERROR_INVALID_SESSION:
		ret = -EPIPE;
		break;
	// TODO: review it
	case STP_ERROR_INVALID_PARAMETERS:
		ret = -EBADF;
		break;
	// TODO: review it
	case STP_ERROR_IO_INTRERRUPT:
		ret = -EPIPE;
		break;
	// TODO: review it
	case STP_ROUTER_ERROR_INVALID_METADATA:
		ret = -EBADF;
		break;
	default:
		break;
	}

	return ret;
}

static int stp_router_wait_read(stp_router_handle handle)
{
	int ret;

	if (handle >= DEFAULT_MAX_CLIENTS)
		return 0;

	ret = wait_for_completion_interruptible(
			&_stp_dev->user_data[handle].read_done);

	return ret;
}

static int stp_router_resume_read(stp_router_handle handle)
{
	if (handle >= DEFAULT_MAX_CLIENTS)
		return 0;

	complete(&_stp_dev->user_data[handle].read_done);

	return 0;
}

static bool stp_router_check_error_read(int error)
{
	/*
	 * -ERESTARTSYS is returned when SoC is in collapse
	 * this error needs to be returned to upper layer
	 */
	if (error == -ERESTARTSYS)
		return true;

	return false;
}

/* Initialize the STP_DEV internal data */
static int stp_dev_populate_internal_data(struct device *dev)
{
	int ret = 0;
	int i;

	if (_stp_dev) {
		pr_err("STP_DEV already initialized\n");

		ret = -1;
		goto error;
	}

	_stp_dev = kzalloc(sizeof(*_stp_dev), GFP_KERNEL);
	if (!_stp_dev) {
		ret = -ENOMEM;
		goto error;
	}

	_stp_dev->dev = dev;
	mutex_init(&_stp_dev->oc_lock);

	for (i = 0; i < DEFAULT_MAX_CLIENTS; i++) {

		mutex_init(&_stp_dev->user_data[i].rx_lock);
		mutex_init(&_stp_dev->user_data[i].tx_lock);

		init_completion(&_stp_dev->user_data[i].read_done);
		init_completion(&_stp_dev->user_data[i].write_done);
	}

	return 0;

error:
	kfree(_stp_dev);
	_stp_dev = NULL;

	return ret;
}

/* Remove STP_DEV internal data */
static int stp_dev_remove_internal_data(void)
{
	int ret = 0;
	int i = 0;

	if (!_stp_dev) {
		pr_err("STP_DEV error: internal data already removed\n");
		return STP_ERROR;
	}

	for (i = 0; i < DEFAULT_MAX_CLIENTS; i++) {
		_stp_dev->user_data[i].inuse = false;

		if (_stp_dev->user_data[i].tx_buffer != NULL) {
			kfree(_stp_dev->user_data[i].tx_buffer);
			_stp_dev->user_data[i].tx_buffer = NULL;
			_stp_dev->user_data[i].tx_len = 0;
		}

		if (_stp_dev->user_data[i].rx_buffer != NULL) {
			kfree(_stp_dev->user_data[i].rx_buffer);
			_stp_dev->user_data[i].rx_buffer = NULL;
			_stp_dev->user_data[i].rx_len = 0;
		}
		if (_stp_dev->user_data[i].router_buffer != NULL) {
			kfree(_stp_dev->user_data[i].router_buffer);
			_stp_dev->user_data[i].router_buffer = NULL;
			_stp_dev->user_data[i].router_buffer_len = 0;
		}
	}

	kfree(_stp_dev);

	_stp_dev = NULL;

	return ret;
}

//---------------------------------------------------------

/* Write-only message with current device setup */
static ssize_t stp_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	ssize_t status;
	unsigned long missing;
	int real_count;
	int pending_count;
	const char *p_buf;
	int ret;
	int stp_ret;

	struct stp_user_data *user_data;

	STP_ASSERT(_stp_dev, "Internal data not initialized!");

	if (!filp || !buf || !f_pos || !filp->private_data) {
		dev_err(_stp_dev->dev, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	user_data = filp->private_data;

	if (count == 0) {
		stp_ret = stp_router_check_for_rw_errors(user_data->stpr_hdl);
		ret = spi_stp_dev_map_errors(stp_ret);
		return ret;
	}

	mutex_lock(&user_data->tx_lock);

	if (!user_data->inuse || user_data->rx_buffer == NULL ||
	    user_data->rx_len == 0 || user_data->tx_buffer == NULL ||
	    user_data->tx_len == 0) {
		dev_err(_stp_dev->dev, "invalid user_data\n");
		mutex_unlock(&user_data->tx_lock);
		return -EINVAL;
	}

	pending_count = count;
	p_buf = buf;
	while (pending_count > 0) {

		unsigned int len = user_data->tx_len;

		if (len > pending_count)
			len = (unsigned int)pending_count;

		missing = copy_from_user(user_data->tx_buffer, p_buf, len);

		if (missing != 0) {
			pr_err("%s: missing = %d\n", __func__, (int)missing);
			status = -EFAULT;
			goto error;
		}

		/* This is a non-blocking call,
		 * since the call should return on close()
		 */

		ret = stp_router_write(user_data->stpr_hdl,
			user_data->tx_buffer,
			len, &real_count);

		if (ret != STP_SUCCESS) {
			status = spi_stp_dev_map_errors(ret);
			goto error;
		}

		if (real_count != len) {
			pr_err("%s: writing error\n", __func__);
			status = -EFAULT;
			goto error;
		}

		p_buf += len;
		pending_count -= len;
	};

	status = count - pending_count;

error:
	mutex_unlock(&user_data->tx_lock);

	return status;
}

/* Read-only message with current device setup */
static ssize_t
stp_dev_read(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	ssize_t status = 0;
	unsigned long missing;
	size_t pending_count;
	char *p;
	int ret;
	int stp_ret;
	struct stp_user_data *user_data;
	unsigned int len;
	int real_count;

	STP_ASSERT(_stp_dev, "Internal data not initialized!");

	if (!filp || !buf || !f_pos || !filp->private_data) {
		dev_err(_stp_dev->dev, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	user_data = filp->private_data;

	if (count == 0) {
		stp_ret = stp_router_check_for_rw_errors(user_data->stpr_hdl);
		ret = spi_stp_dev_map_errors(stp_ret);
		return ret;
	}

	mutex_lock(&user_data->rx_lock);

	if (!user_data->inuse || user_data->rx_buffer == NULL ||
	    user_data->rx_len == 0 || user_data->tx_buffer == NULL ||
	    user_data->tx_len == 0) {
		dev_err(_stp_dev->dev, "invalid user_data\n");
		mutex_unlock(&user_data->rx_lock);
		return -EINVAL;
	}

	pending_count = count;
	p = buf;
	while (pending_count > 0) {
		len = user_data->rx_len;

		if (len > pending_count)
			len = (unsigned int)pending_count;

		ret = stp_router_read(user_data->stpr_hdl, user_data->rx_buffer,
				len, &real_count);

		if (ret != STP_ROUTER_SUCCESS) {
			pr_err("stp_router_read error = %d\n", ret);
			status = spi_stp_dev_map_errors(ret);
			goto error;
		}

		if (real_count != len) {
			pr_err("stp_router_read read len error %d:%d\n", len,
				real_count);
			status = -EFAULT;
			goto error;
		}

		missing = copy_to_user(p, user_data->rx_buffer,
			len);

		if (missing != 0) {
			pr_err("%s: missing = %d\n", __func__, (int)missing);
			status = -EFAULT;
			goto error;
		}

		p += len;
		pending_count -= len;
	}

	status = count - pending_count;

error:
	mutex_unlock(&user_data->rx_lock);

	return status;
}

static int stp_dev_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	unsigned int minor = 0;

	struct stp_router_open_t stpro;

	STP_ASSERT(_stp_dev, "Internal data not initialized!");

	if (!filp || !inode) {
		dev_err(_stp_dev->dev, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&_stp_dev->oc_lock);

	minor = iminor(inode);
	if (minor >= DEFAULT_MAX_CLIENTS || _stp_dev->user_data[minor].inuse) {
		dev_err(_stp_dev->dev, "max users reached or device already open\n");
		ret = -EPERM;
		goto err_max_users;
	}

	_stp_dev->user_data[minor].tx_buffer =
		kzalloc(SIZE_STP_DEV_BUFFER, GFP_KERNEL);
	if (!_stp_dev->user_data[minor].tx_buffer) {
		dev_err(_stp_dev->dev, "open user (%d) ENOMEM for tx_buf\n",
		minor);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	_stp_dev->user_data[minor].rx_buffer =
		kzalloc(SIZE_STP_DEV_BUFFER, GFP_KERNEL);
	if (!_stp_dev->user_data[minor].rx_buffer) {
		dev_err(_stp_dev->dev, "open user (%d) ENOMEM for rx_buf\n",
			minor);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	_stp_dev->user_data[minor].router_buffer =
		kzalloc(SIZE_STP_ROUTER_RX_PIPELINE, GFP_KERNEL);
	if (!_stp_dev->user_data[minor].router_buffer) {
		dev_err(_stp_dev->dev, "open user (%d) ENOMEM for router_buf\n",
			minor);
		ret = -ENOMEM;
		goto err_mem_alloc;
	}

	_stp_dev->user_data[minor].tx_len = SIZE_STP_DEV_BUFFER;
	_stp_dev->user_data[minor].rx_len = SIZE_STP_DEV_BUFFER;
	_stp_dev->user_data[minor].router_buffer_len =
		SIZE_STP_ROUTER_RX_PIPELINE;

	stpro.client_id = minor;
	stpro.rx_buffer = _stp_dev->user_data[minor].router_buffer;
	stpro.rx_buffer_size = _stp_dev->user_data[minor].router_buffer_len;

	if (stp_router_open(&_stp_dev->user_data[minor].stpr_hdl, &stpro) !=
		STP_ROUTER_SUCCESS) {
		ret = -EPERM;
		goto err_mem_alloc;
	}

	_stp_dev->user_data[minor].inuse = true;

	filp->private_data = &(_stp_dev->user_data)[minor];
	dev_info(_stp_dev->dev, "%s: user_data = 0x%p\n",
		__func__, filp->private_data);

	mutex_unlock(&_stp_dev->oc_lock);

	return ret;

err_mem_alloc:
	kfree(_stp_dev->user_data[minor].router_buffer);
	_stp_dev->user_data[minor].router_buffer = NULL;
	_stp_dev->user_data[minor].router_buffer_len = 0;

	kfree(_stp_dev->user_data[minor].rx_buffer);
	_stp_dev->user_data[minor].rx_buffer = NULL;
	_stp_dev->user_data[minor].rx_len = 0;

	kfree(_stp_dev->user_data[minor].tx_buffer);
	_stp_dev->user_data[minor].tx_buffer = NULL;
	_stp_dev->user_data[minor].tx_len = 0;

err_max_users:
	mutex_unlock(&_stp_dev->oc_lock);
	return ret;
}

static int stp_dev_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct stp_user_data *user_data;

	STP_ASSERT(_stp_dev, "Internal data not initialized!");

	if (!filp || !inode || !filp->private_data) {
		dev_err(_stp_dev->dev, "%s: invalid parameters\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&_stp_dev->oc_lock);

	user_data = filp->private_data;
	stp_router_close(user_data->stpr_hdl);
	user_data->inuse = false;

	kfree(user_data->tx_buffer);
	user_data->tx_buffer = NULL;
	user_data->tx_len = 0;

	kfree(user_data->rx_buffer);
	user_data->rx_buffer = NULL;
	user_data->rx_len = 0;

	kfree(user_data->router_buffer);
	user_data->router_buffer = NULL;
	user_data->router_buffer_len = 0;

	filp->private_data = NULL;

	mutex_unlock(&_stp_dev->oc_lock);

	return ret;
}

//---------------------------------------------------------

static const struct file_operations stp_dev_fops = {
	.owner =				THIS_MODULE,
	.write =				stp_dev_write,
	.read =				stp_dev_read,
	.open =				stp_dev_open,
	.release =			stp_dev_release,
	.llseek =			no_llseek,
};

//-----------------------------------------------------------------

static void stp_dev_start(struct device *dev)
{
	struct spi_device *spi = to_spi_device(_stp_dev->dev);
	int ret;
	u16 cur_mode;

	if (!_stp_dev) {
		dev_err(dev, "%s: ERROR stp_dev is null\n", __func__);
		return;
	}

	cur_mode = spi->mode;
	spi->mode = SPI_MODE_1;
	ret = spi_setup(spi);
}

static ssize_t stp_start_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", stp_start);
}

static ssize_t stp_start_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	int value;

	if (kstrtoint(buf, 10, &value))
		return 0;

	if (value == stp_start)
		return count;

	stp_start = value;

	if (stp_start == 1) {
		spi_stp_signal_start();
		stp_dev_start(dev);
	}

	return count;
}

int stp_dev_stp_start(void)
{
	return stp_start;
}

static ssize_t stp_stats_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct stp_statistics stats;

	stp_get(STP_STATS, &stats);

	return scnprintf(buf, PAGE_SIZE,
		"TX transactions: %lu\n"
		"TX data: %lu\n"
		"TX pipeline data: %lu\n"
		"RX transactions: %lu\n"
		"RX data: %lu\n"
		"RX pipeline data: %lu\n",
		stats.tx_stats.total_transactions,
		stats.tx_stats.total_data,
		stats.tx_stats.total_data_pipeline,
		stats.rx_stats.total_transactions,
		stats.rx_stats.total_data,
		stats.rx_stats.total_data_pipeline);
}

static ssize_t stp_debug_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	unsigned int stp_tx_data;
	unsigned int stp_rx_data;
	unsigned int stp_synced;
	unsigned int stp_valid_session;
	unsigned int stp_slave_connected;
	unsigned int stp_master_connected;
	unsigned int stp_gpio_slave_ready;
	unsigned int stp_gpio_slave_data;

	stp_get(STP_TX_DATA, &stp_tx_data);
	stp_get(STP_RX_DATA, &stp_rx_data);
	stp_get(STP_ATTRIB_SYNCED, &stp_synced);
	stp_get(STP_ATTRIB_VALID_SESSION, &stp_valid_session);
	stp_get(STP_ATTRIB_SLAVE_CONNECTED, &stp_slave_connected);
	stp_get(STP_ATTRIB_MASTER_CONNECTED, &stp_master_connected);

	stp_gpio_slave_ready = is_mcu_ready_to_receive_data();
	stp_gpio_slave_data = is_mcu_data_available();

	return scnprintf(buf, PAGE_SIZE,
		"Synced: %d\n"
		"Valid session: %d\n"
		"Master connected: %d\n"
		"Slave connected: %d\n"
		"Wait for read: %d\n"
		"Wait for write: %d\n"
		"Wait for slave: %d\n"
		"IRQ slave ready: %d\n"
		"GPIO slave ready: %d\n"
		"Wait for data: %d\n"
		"IRQ slave data: %d\n"
		"GPIO slave data: %d\n"
		"Pending TX data: %d\n"
		"Pending RX data: %d\n"
		"IRQ slave ready counter: %lu\n"
		"IRQ slave has data counter: %lu\n"
		"Router droppings pipeline full counter: %lu\n"
		"MCU Ready timer expired counter (IRQ missed): %d\n",
		stp_synced,
		stp_valid_session,
		stp_master_connected,
		stp_slave_connected,
		_stp_debug.wait_for_read,
		_stp_debug.wait_for_write,
		_stp_debug.wait_for_slave,
		_stp_debug.signal_slave_ready,
		stp_gpio_slave_ready,
		_stp_debug.wait_for_data,
		_stp_debug.signal_data,
		stp_gpio_slave_data,
		stp_tx_data,
		stp_rx_data,
		_stp_debug.irq_slave_ready_counter,
		_stp_debug.irq_slave_has_data_counter,
		_stp_debug.router_drops_full_pipeline,
		stp_mcu_ready_timer_expired_irq_missed_counter);
}

static ssize_t stprouter_debug_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	return stp_router_debug(buf, PAGE_SIZE);
}

static DEVICE_ATTR_RW(stp_start);
static DEVICE_ATTR_RO(stp_stats);
static DEVICE_ATTR_RO(stp_debug);
static DEVICE_ATTR_RO(stprouter_debug);

static int stp_dev_create_device(
	const struct file_operations *fops)
{
	int ret = STP_SUCCESS;
	int dynamic_major;
	unsigned long minor;
	char stp_dev_name[32];
	struct device *dev;

	BUILD_BUG_ON(N_STP_DEV_MINORS > 256);
	dynamic_major = register_chrdev(STP_DEV_MAJOR,
		STP_DEV_DEVICE_NAME, fops);

	if (dynamic_major < 0) {
		pr_err("register_chrdev error (%d)\n",
			dynamic_major);
		ret = STP_ERROR;
		return ret;
	}

	_stp_dev->dev_class = class_create(THIS_MODULE, STP_DEV_DEVICE_NAME);
	if (!_stp_dev->dev_class)
		goto error;

	minor = find_first_zero_bit(minors, N_STP_DEV_MINORS);
	if (minor < N_STP_DEV_MINORS) {
		_stp_dev->devt = MKDEV(dynamic_major, minor);
		_stp_dev->stp_dev = device_create(_stp_dev->dev_class,
				_stp_dev->dev, _stp_dev->devt, NULL,
				STP_DEV_DEVICE_NAME);

		if (IS_ERR(_stp_dev->stp_dev)) {
			pr_err("stp_dev: failed on device_create %ld\n",
				PTR_ERR(_stp_dev->stp_dev));
			goto error;
		} else {
			set_bit(minor, minors);
		}

		device_create_file(_stp_dev->stp_dev,
			&dev_attr_stp_start);

		device_create_file(_stp_dev->stp_dev,
			&dev_attr_stp_stats);

		device_create_file(_stp_dev->stp_dev,
			&dev_attr_stp_debug);

		device_create_file(_stp_dev->stp_dev,
			&dev_attr_stprouter_debug);
	}

	/* create the second stp device */
	minor = find_first_zero_bit(minors, N_STP_DEV_MINORS);
	snprintf(stp_dev_name, sizeof(stp_dev_name), "stp%lul", minor);
	dev = device_create(_stp_dev->dev_class, _stp_dev->dev,
		MKDEV(dynamic_major, minor), NULL, "%s", stp_dev_name);
	if (!IS_ERR(dev)) {
		pr_info("%s: created device /dev/%s\n", __func__,
			stp_dev_name);
		set_bit(minor, minors);
	} else {
		pr_err("%s: error creating device /dev/%s (%ld)\n",
			__func__, stp_dev_name, PTR_ERR(dev));
	}

	return STP_SUCCESS;

error:
	unregister_chrdev(STP_DEV_MAJOR, STP_DEV_DEVICE_NAME);
	return ret;
}

static int stp_dev_remove_device(void)
{
	int ret = STP_SUCCESS;

	device_remove_file(_stp_dev->stp_dev,
		&dev_attr_stprouter_debug);

	device_remove_file(_stp_dev->stp_dev,
		&dev_attr_stp_debug);

	device_remove_file(_stp_dev->stp_dev,
		&dev_attr_stp_stats);

	device_remove_file(_stp_dev->stp_dev,
		&dev_attr_stp_start);

	device_destroy(_stp_dev->dev_class, _stp_dev->devt);

	class_destroy(_stp_dev->dev_class);

	unregister_chrdev(STP_DEV_MAJOR, STP_DEV_DEVICE_NAME);

	return ret;
}

int stp_dev_init(struct device *dev)
{
	int ret = STP_SUCCESS;

	struct stp_router_wait_signal_table signal_table = {
		&stp_router_wait_read,
		&stp_router_resume_read,
		&stp_router_check_error_read,
	};

	struct stp_router_init_t router_init;

	ret = stp_dev_populate_internal_data(dev);
	if (ret != STP_SUCCESS)
		return ret;

	ret = stp_dev_create_device(&stp_dev_fops);
	if (ret != STP_SUCCESS)
		goto error;

	router_init.wait_signal = &signal_table;
	ret = stp_router_init(&router_init);
	if (ret != STP_ROUTER_SUCCESS)
		goto error;

	return STP_SUCCESS;

error:
	stp_dev_remove_internal_data();
	return ret;
}

int stp_dev_remove(void)
{
	int ret = STP_SUCCESS;

	ret = stp_dev_remove_device();
	if (ret != STP_SUCCESS)
		goto error;

	ret = stp_dev_remove_internal_data();
	if (ret != STP_SUCCESS)
		goto error;

error:
	return ret;
}
