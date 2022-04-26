// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#include <linux/compat.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include "spi_stp_dev.h"

#define STP_RAW_DEV_MAJOR		0	/* dynamic */
#define N_STP_RAW_DEV_MINORS		32

static DECLARE_BITMAP(minors, N_STP_RAW_DEV_MINORS);

/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
				| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
				| SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
				| SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)


#define STP_RAW_DEVICE_NAME	"stp_raw"

struct stp_raw_data {
	dev_t			devt;
	spinlock_t		spi_lock;
	struct spi_device	*spi;
	struct list_head	device_entry;

	/* TX/RX buffers are NULL unless this device is open (users > 0) */
	struct mutex		buf_lock;
	unsigned int		users;
	u8			*tx_buffer;
	u8			*rx_buffer;
	u32			speed_hz;

};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static unsigned int bufsiz = 4096;
module_param(bufsiz, uint, 0444);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

/*-------------------------------------------------------------------------*/

static ssize_t
stp_raw_sync(struct stp_raw_data *stp_raw, struct spi_message *message)
{
	DECLARE_COMPLETION_ONSTACK(done);
	int status;
	struct spi_device *spi;

	spin_lock_irq(&stp_raw->spi_lock);
	spi = stp_raw->spi;
	spin_unlock_irq(&stp_raw->spi_lock);

	if (spi == NULL)
		status = -ESHUTDOWN;
	else
		status = spi_sync(spi, message);

	if (status == 0)
		status = message->actual_length;

	return status;
}

static inline ssize_t
stp_raw_sync_write(struct stp_raw_data *stp_raw, size_t len)
{
	struct spi_transfer	t = {
			.tx_buf		= stp_raw->tx_buffer,
			.len		= len,
			.speed_hz	= stp_raw->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return stp_raw_sync(stp_raw, &m);
}

static inline ssize_t
stp_raw_sync_read(struct stp_raw_data *stp_raw, size_t len)
{
	struct spi_transfer	t = {
			.rx_buf		= stp_raw->rx_buffer,
			.len		= len,
			.speed_hz	= stp_raw->speed_hz,
		};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return stp_raw_sync(stp_raw, &m);
}

/*-------------------------------------------------------------------------*/

static ssize_t
stp_raw_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct stp_raw_data	*stp_raw;
	ssize_t			status = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	stp_raw = filp->private_data;

	mutex_lock(&stp_raw->buf_lock);
	status = stp_raw_sync_read(stp_raw, count);
	if (status > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, stp_raw->rx_buffer, status);
		if (missing == status)
			status = -EFAULT;
		else
			status = status - missing;
	}
	mutex_unlock(&stp_raw->buf_lock);

	return status;
}

static ssize_t
stp_raw_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct stp_raw_data	*stp_raw;
	ssize_t			status = 0;
	unsigned long		missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	stp_raw = filp->private_data;

	mutex_lock(&stp_raw->buf_lock);
	missing = copy_from_user(stp_raw->tx_buffer, buf, count);
	if (missing == 0)
		status = stp_raw_sync_write(stp_raw, count);
	else
		status = -EFAULT;
	mutex_unlock(&stp_raw->buf_lock);

	return status;
}

static int stp_raw_message(struct stp_raw_data *stp_raw,
		struct spi_ioc_transfer *u_xfers, unsigned int n_xfers)
{
	struct spi_message	msg;
	struct spi_transfer	*k_xfers;
	struct spi_transfer	*k_tmp;
	struct spi_ioc_transfer *u_tmp;
	unsigned int		n, total, tx_total, rx_total;
	u8			*tx_buf, *rx_buf;
	int			status = -EFAULT;

	spi_message_init(&msg);
	k_xfers = kcalloc(n_xfers, sizeof(*k_tmp), GFP_KERNEL);
	if (k_xfers == NULL)
		return -ENOMEM;

	/* Construct spi_message, copying any tx data to bounce buffer.
	 * We walk the array of user-provided transfers, using each one
	 * to initialize a kernel version of the same transfer.
	 */
	tx_buf = stp_raw->tx_buffer;
	rx_buf = stp_raw->rx_buffer;
	total = 0;
	tx_total = 0;
	rx_total = 0;
	for (n = n_xfers, k_tmp = k_xfers, u_tmp = u_xfers;
			n;
			n--, k_tmp++, u_tmp++) {
		k_tmp->len = u_tmp->len;

		total += k_tmp->len;
		/* Since the function returns the total length of transfers
		 * on success, restrict the total to positive int values to
		 * avoid the return value looking like an error.  Also check
		 * each transfer length to avoid arithmetic overflow.
		 */
		if (total > INT_MAX || k_tmp->len > INT_MAX) {
			status = -EMSGSIZE;
			goto done;
		}

		if (u_tmp->rx_buf) {
			/* this transfer needs space in RX bounce buffer */
			rx_total += k_tmp->len;
			if (rx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->rx_buf = rx_buf;
			if (!access_ok(VERIFY_WRITE, (u8 __user *)
						(uintptr_t) u_tmp->rx_buf,
						u_tmp->len))
				goto done;
			rx_buf += k_tmp->len;
		}
		if (u_tmp->tx_buf) {
			/* this transfer needs space in TX bounce buffer */
			tx_total += k_tmp->len;
			if (tx_total > bufsiz) {
				status = -EMSGSIZE;
				goto done;
			}
			k_tmp->tx_buf = tx_buf;
			if (copy_from_user(tx_buf, (const u8 __user *)
						(uintptr_t) u_tmp->tx_buf,
					u_tmp->len))
				goto done;
			tx_buf += k_tmp->len;
		}

		k_tmp->cs_change = !!u_tmp->cs_change;
		k_tmp->tx_nbits = u_tmp->tx_nbits;
		k_tmp->rx_nbits = u_tmp->rx_nbits;
		k_tmp->bits_per_word = u_tmp->bits_per_word;
		k_tmp->delay_usecs = u_tmp->delay_usecs;
		k_tmp->speed_hz = u_tmp->speed_hz;
		if (!k_tmp->speed_hz)
			k_tmp->speed_hz = stp_raw->speed_hz;
#ifdef VERBOSE
		dev_dbg(&stp_raw->spi->dev,
			"  xfer len %u %s%s%s%dbits %u usec %uHz\n",
			u_tmp->len,
			u_tmp->rx_buf ? "rx " : "",
			u_tmp->tx_buf ? "tx " : "",
			u_tmp->cs_change ? "cs " : "",
			u_tmp->bits_per_word ? : stp_raw->spi->bits_per_word,
			u_tmp->delay_usecs,
			u_tmp->speed_hz ? : stp_raw->spi->max_speed_hz);
#endif
		spi_message_add_tail(k_tmp, &msg);
	}

	status = stp_raw_sync(stp_raw, &msg);
	if (status < 0)
		goto done;

	/* copy any rx data out of bounce buffer */
	rx_buf = stp_raw->rx_buffer;
	for (n = n_xfers, u_tmp = u_xfers; n; n--, u_tmp++) {
		if (u_tmp->rx_buf) {
			if (__copy_to_user((u8 __user *)
					(uintptr_t) u_tmp->rx_buf, rx_buf,
					u_tmp->len)) {
				status = -EFAULT;
				goto done;
			}
			rx_buf += u_tmp->len;
		}
	}
	status = total;

done:
	kfree(k_xfers);
	return status;
}

static struct spi_ioc_transfer *
stp_raw_get_ioc_message(unsigned int cmd, struct spi_ioc_transfer __user *u_ioc,
		unsigned int *n_ioc)
{
	struct spi_ioc_transfer	*ioc;
	u32	tmp;

	/* Check type, command number and direction */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC
			|| _IOC_NR(cmd) != _IOC_NR(SPI_IOC_MESSAGE(0))
			|| _IOC_DIR(cmd) != _IOC_WRITE)
		return ERR_PTR(-ENOTTY);

	tmp = _IOC_SIZE(cmd);
	if ((tmp % sizeof(struct spi_ioc_transfer)) != 0)
		return ERR_PTR(-EINVAL);
	*n_ioc = tmp / sizeof(struct spi_ioc_transfer);
	if (*n_ioc == 0)
		return NULL;

	/* copy into scratch area */
	ioc = kmalloc(tmp, GFP_KERNEL);
	if (!ioc)
		return ERR_PTR(-ENOMEM);
	if (__copy_from_user(ioc, u_ioc, tmp)) {
		kfree(ioc);
		return ERR_PTR(-EFAULT);
	}
	return ioc;
}

static long
stp_raw_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int			err = 0;
	int			retval = 0;
	struct stp_raw_data	*stp_raw;
	struct spi_device	*spi;
	u32			tmp;
	unsigned int		n_ioc;
	struct spi_ioc_transfer	*ioc;

	/* Check type and command number */
	if (_IOC_TYPE(cmd) != SPI_IOC_MAGIC)
		return -ENOTTY;

	/* Check access direction once here; don't repeat below.
	 * IOC_DIR is from the user perspective, while access_ok is
	 * from the kernel perspective; so they look reversed.
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err == 0 && _IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
				(void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	stp_raw = filp->private_data;
	spin_lock_irq(&stp_raw->spi_lock);
	spi = spi_dev_get(stp_raw->spi);
	spin_unlock_irq(&stp_raw->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* use the buffer lock here for triple duty:
	 *  - prevent I/O (from us) so calling spi_setup() is safe;
	 *  - prevent concurrent SPI_IOC_WR_* from morphing
	 *    data fields while SPI_IOC_RD_* reads them;
	 *  - SPI_IOC_MESSAGE needs the buffer locked "normally".
	 */
	mutex_lock(&stp_raw->buf_lock);

	switch (cmd) {
	/* read requests */
	case SPI_IOC_RD_MODE:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MODE32:
		retval = __put_user(spi->mode & SPI_MODE_MASK,
					(__u32 __user *)arg);
		break;
	case SPI_IOC_RD_LSB_FIRST:
		retval = __put_user((spi->mode & SPI_LSB_FIRST) ?  1 : 0,
					(__u8 __user *)arg);
		break;
	case SPI_IOC_RD_BITS_PER_WORD:
		retval = __put_user(spi->bits_per_word, (__u8 __user *)arg);
		break;
	case SPI_IOC_RD_MAX_SPEED_HZ:
		retval = __put_user(stp_raw->speed_hz, (__u32 __user *)arg);
		break;

	/* write requests */
	case SPI_IOC_WR_MODE:
	case SPI_IOC_WR_MODE32:
		if (cmd == SPI_IOC_WR_MODE)
			retval = __get_user(tmp, (u8 __user *)arg);
		else
			retval = __get_user(tmp, (u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp & ~SPI_MODE_MASK) {
				retval = -EINVAL;
				break;
			}

			tmp |= spi->mode & ~SPI_MODE_MASK;
			spi->mode = (u16)tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "spi mode %x\n", tmp);
		}
		break;
	case SPI_IOC_WR_LSB_FIRST:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u32	save = spi->mode;

			if (tmp)
				spi->mode |= SPI_LSB_FIRST;
			else
				spi->mode &= ~SPI_LSB_FIRST;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->mode = save;
			else
				dev_dbg(&spi->dev, "%csb first\n",
						tmp ? 'l' : 'm');
		}
		break;
	case SPI_IOC_WR_BITS_PER_WORD:
		retval = __get_user(tmp, (__u8 __user *)arg);
		if (retval == 0) {
			u8	save = spi->bits_per_word;

			spi->bits_per_word = tmp;
			retval = spi_setup(spi);
			if (retval < 0)
				spi->bits_per_word = save;
			else
				dev_dbg(&spi->dev, "%d bits per word\n", tmp);
		}
		break;
	case SPI_IOC_WR_MAX_SPEED_HZ:
		retval = __get_user(tmp, (__u32 __user *)arg);
		if (retval == 0) {
			u32	save = spi->max_speed_hz;

			spi->max_speed_hz = tmp;
			retval = spi_setup(spi);
			if (retval >= 0)
				stp_raw->speed_hz = tmp;
			else
				dev_dbg(&spi->dev, "%d Hz (max)\n", tmp);
			spi->max_speed_hz = save;
		}
		break;

	default:
		/* segmented and/or full-duplex I/O request */
		/* Check message and copy into scratch area */
		ioc = stp_raw_get_ioc_message(cmd,
				(struct spi_ioc_transfer __user *)arg, &n_ioc);
		if (IS_ERR(ioc)) {
			retval = PTR_ERR(ioc);
			break;
		}
		if (!ioc)
			break;	/* n_ioc is also 0 */

		/* translate to spi_message, execute */
		retval = stp_raw_message(stp_raw, ioc, n_ioc);
		kfree(ioc);
		break;
	}

	mutex_unlock(&stp_raw->buf_lock);
	spi_dev_put(spi);
	return retval;
}

#ifdef CONFIG_COMPAT
static long
stp_raw_compat_ioc_message(struct file *filp, unsigned int cmd,
		unsigned long arg)
{
	struct spi_ioc_transfer __user	*u_ioc;
	int				retval = 0;
	struct stp_raw_data		*stp_raw;
	struct spi_device		*spi;
	unsigned int			n_ioc, n;
	struct spi_ioc_transfer		*ioc;

	u_ioc = (struct spi_ioc_transfer __user *) compat_ptr(arg);
	if (!access_ok(VERIFY_READ, u_ioc, _IOC_SIZE(cmd)))
		return -EFAULT;

	/* guard against device removal before, or while,
	 * we issue this ioctl.
	 */
	stp_raw = filp->private_data;
	spin_lock_irq(&stp_raw->spi_lock);
	spi = spi_dev_get(stp_raw->spi);
	spin_unlock_irq(&stp_raw->spi_lock);

	if (spi == NULL)
		return -ESHUTDOWN;

	/* SPI_IOC_MESSAGE needs the buffer locked "normally" */
	mutex_lock(&stp_raw->buf_lock);

	/* Check message and copy into scratch area */
	ioc = stp_raw_get_ioc_message(cmd, u_ioc, &n_ioc);
	if (IS_ERR(ioc)) {
		retval = PTR_ERR(ioc);
		goto done;
	}
	if (!ioc)
		goto done;	/* n_ioc is also 0 */

	/* Convert buffer pointers */
	for (n = 0; n < n_ioc; n++) {
		ioc[n].rx_buf = (uintptr_t) compat_ptr(ioc[n].rx_buf);
		ioc[n].tx_buf = (uintptr_t) compat_ptr(ioc[n].tx_buf);
	}

	/* translate to spi_message, execute */
	retval = stp_raw_message(stp_raw, ioc, n_ioc);
	kfree(ioc);

done:
	mutex_unlock(&stp_raw->buf_lock);
	spi_dev_put(spi);
	return retval;
}

static long
stp_raw_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	if (_IOC_TYPE(cmd) == SPI_IOC_MAGIC
			&& _IOC_NR(cmd) == _IOC_NR(SPI_IOC_MESSAGE(0))
			&& _IOC_DIR(cmd) == _IOC_WRITE)
		return stp_raw_compat_ioc_message(filp, cmd, arg);

	return stp_raw_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define stp_raw_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

static int stp_raw_open(struct inode *inode, struct file *filp)
{
	struct stp_raw_data	*stp_raw;
	int			status = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(stp_raw, &device_list, device_entry) {
		if (stp_raw->devt == inode->i_rdev) {
			status = 0;
			break;
		}
	}

	if (status) {
		pr_debug("stp_raw: nothing for minor %d\n", iminor(inode));
		goto err_find_dev;
	}

	if (!stp_raw->tx_buffer) {
		stp_raw->tx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!stp_raw->tx_buffer) {
			status = -ENOMEM;
			goto err_find_dev;
		}
	}

	if (!stp_raw->rx_buffer) {
		stp_raw->rx_buffer = kmalloc(bufsiz, GFP_KERNEL);
		if (!stp_raw->rx_buffer) {
			status = -ENOMEM;
			goto err_alloc_rx_buf;
		}
	}

	stp_raw->users++;
	filp->private_data = stp_raw;
	nonseekable_open(inode, filp);

	dev_dbg(&stp_raw->spi->dev,
		"%s users = %d, spi mode = %d, bpw = %d, max_speed_hz = %d\n",
		__func__, stp_raw->users, stp_raw->spi->mode,
		stp_raw->spi->bits_per_word, stp_raw->spi->max_speed_hz);

	mutex_unlock(&device_list_lock);
	return 0;

err_alloc_rx_buf:
	kfree(stp_raw->tx_buffer);
	stp_raw->tx_buffer = NULL;
err_find_dev:
	mutex_unlock(&device_list_lock);
	return status;
}

static int stp_raw_release(struct inode *inode, struct file *filp)
{
	struct stp_raw_data	*stp_raw;

	mutex_lock(&device_list_lock);
	stp_raw = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	stp_raw->users--;

	dev_dbg(&stp_raw->spi->dev,
		"%s users = %d, spi mode = %d, bpw = %d, max_speed_hz = %d\n",
		__func__, stp_raw->users, stp_raw->spi->mode,
		stp_raw->spi->bits_per_word, stp_raw->spi->max_speed_hz);

	if (!stp_raw->users) {
		int		dofree;

		kfree(stp_raw->tx_buffer);
		stp_raw->tx_buffer = NULL;

		kfree(stp_raw->rx_buffer);
		stp_raw->rx_buffer = NULL;

		spin_lock_irq(&stp_raw->spi_lock);
		if (stp_raw->spi)
			stp_raw->speed_hz = stp_raw->spi->max_speed_hz;

		/* ... after we unbound from the underlying device? */
		dofree = (stp_raw->spi == NULL);
		spin_unlock_irq(&stp_raw->spi_lock);

		if (dofree)
			kfree(stp_raw);
	}
	mutex_unlock(&device_list_lock);

	return 0;
}

static const struct file_operations stp_raw_fops = {
	.owner =	THIS_MODULE,
	.write =	stp_raw_write,
	.read =		stp_raw_read,
	.unlocked_ioctl = stp_raw_ioctl,
	.compat_ioctl = stp_raw_compat_ioctl,
	.open =		stp_raw_open,
	.release =	stp_raw_release,
	.llseek =	no_llseek,
};

static struct class *stp_raw_class;

static int stp_raw_dev_create(struct stp_raw_data *stp_raw)
{
	int status;
	int dynamic_major;
	unsigned long minor;

	BUILD_BUG_ON(N_STP_RAW_DEV_MINORS > 256);
	dynamic_major = register_chrdev(STP_RAW_DEV_MAJOR,
		STP_RAW_DEVICE_NAME, &stp_raw_fops);

	if (dynamic_major < 0) {
		pr_err("register_chrdev error (%d)\n",
			dynamic_major);
		return dynamic_major;
	}

	stp_raw_class = class_create(THIS_MODULE, STP_RAW_DEVICE_NAME);
	if (IS_ERR(stp_raw_class)) {
		unregister_chrdev(STP_RAW_DEV_MAJOR, STP_RAW_DEVICE_NAME);
		return PTR_ERR(stp_raw_class);
	}

	mutex_lock(&device_list_lock);

	minor = find_first_zero_bit(minors, N_STP_RAW_DEV_MINORS);
	if (minor < N_STP_RAW_DEV_MINORS) {
		struct device *dev;

		stp_raw->devt = MKDEV(dynamic_major, minor);
		dev = device_create(stp_raw_class, &stp_raw->spi->dev,
			stp_raw->devt, stp_raw, STP_RAW_DEVICE_NAME);
		status = PTR_ERR_OR_ZERO(dev);
	} else {
		dev_dbg(&stp_raw->spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}

	if (status == 0) {
		set_bit(minor, minors);
		list_add(&stp_raw->device_entry, &device_list);
	}
	mutex_unlock(&device_list_lock);

	return status;
}

int stp_raw_dev_init(struct spi_device *spi)
{
	struct stp_raw_data	*stp_raw;
	int			status;

	/* Allocate driver data */
	stp_raw = kzalloc(sizeof(*stp_raw), GFP_KERNEL);
	if (!stp_raw)
		return -ENOMEM;

	/* Initialize the driver data */
	stp_raw->spi = spi;
	stp_raw->speed_hz = spi->max_speed_hz;
	spin_lock_init(&stp_raw->spi_lock);
	mutex_init(&stp_raw->buf_lock);

	INIT_LIST_HEAD(&stp_raw->device_entry);

	status = stp_raw_dev_create(stp_raw);
	if (status != 0)
		goto error;

	return status;

error:
	kfree(stp_raw);
	return status;
}

int stp_raw_dev_remove(void)
{
	struct stp_raw_data	*stp_raw;

	mutex_lock(&device_list_lock);

	list_for_each_entry(stp_raw, &device_list, device_entry) {
		/* make sure ops on existing fds can abort cleanly */
		spin_lock_irq(&stp_raw->spi_lock);
		stp_raw->spi = NULL;
		spin_unlock_irq(&stp_raw->spi_lock);

		list_del(&stp_raw->device_entry);
		device_destroy(stp_raw_class, stp_raw->devt);
		clear_bit(MINOR(stp_raw->devt), minors);
		if (stp_raw->users == 0)
			kfree(stp_raw);
	}

	mutex_unlock(&device_list_lock);
	return 0;
}
