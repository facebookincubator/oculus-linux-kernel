// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/ktime.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/cpufreq.h>
#include <linux/pm_wakeup.h>
#include "gf_spi.h"
#include "gf_common.h"

#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#include <linux/async.h>

#define GF_DRIVER_VER "goodix,V01"

#define GF_SPIDEV_NAME     "goodix,gw39a1"
/*device name after register in charater*/
#define GF_DEV_NAME         "goodix_fp"
#define	GF_INPUT_NAME	    "goodix_fp_key"	/*"goodix_fp" */

#define	CHRD_DRIVER_NAME	"goodix_fp_spi"
#define	CLASS_NAME		    "goodix_fp"
#define SPIDEV_MAJOR		225
#define N_SPI_MINORS		32

/*GF regs*/
#define GF_CHIP_ID_LO				0x0000
#define GF_CHIP_ID_HI				0x0002
#define GF_VENDOR_ID				0x0006
#define MILAN_REG_CHIP_STATE		0x000e
#define GF_IRQ_CTRL1				0x0120 // LGE_CHANGES - for pin check test
#define GF_IRQ_CTRL2				0x0124
#define GF_IRQ_CTRL3				0x0126
#define GF_IRQ_CTRL4				0x0128 // LGE_CHANGES - for pin check test

/*GF input keys*/
struct gf_key_map key_map[] = {
	{  "POWER",	KEY_POWER  },
	{  "HOME",	KEY_HOME   },
	{  "MENU",	KEY_MENU   },
	{  "BACK",	KEY_BACK   },
	{  "UP",	KEY_UP     },
	{  "DOWN",	KEY_DOWN   },
	{  "LEFT",	KEY_LEFT   },
	{  "RIGHT",	KEY_RIGHT  },
	{  "FORCE",	KEY_F9     },
	{  "CLICK",	KEY_F19    },
};

/**************************debug******************************/

#define MAX_RETRY_HW_CHECK_COUNT (1)

#define MAX_REST_RETRY_COUNT 3
#define MAX_IRQ_PIN_CHECK_RETRY_COUNT 10

/*Global variables*/
static DECLARE_BITMAP(minors, N_SPI_MINORS);
static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

static struct gf_dev gf;
static unsigned int bufsiz = 22180;
static unsigned int gf_spi_speed[GF_SPI_KEEP_SPEED] = {4800000, 4800000};
static struct class *gf_class;
static struct spi_driver gf_driver;
static int gf_probe_fail;


/******************* Enable/Disable IRQ Start ****************/
static void gf_enable_irq(struct gf_dev *gf_dev)
{
	FUNC_ENTRY();
	if (gf_dev->irq_enabled) {
		gf_dbg("IRQ has been enabled.\n");
	} else {
		enable_irq(gf_dev->irq);
		gf_dev->irq_enabled = 1;
	}
	FUNC_EXIT();
}

static void gf_disable_irq(struct gf_dev *gf_dev)
{
	FUNC_ENTRY();
	if (gf_dev->irq_enabled) {
		gf_dev->irq_enabled = 0;
		disable_irq(gf_dev->irq);
	} else {
		gf_dbg("IRQ has been disabled.\n");
	}
	FUNC_EXIT();
}
/******************* Enable/Disable IRQ End ****************/


#ifdef DEVICE_SPI_TEST
static int gf_get_hw_id(struct gf_dev *gf_dev)
{
	int i;

	for (i = 0; i < MAX_RETRY_HW_CHECK_COUNT; i++) {
		gf_spi_read_data(gf_dev, 0x0000, 4, gf_dev->gBuffer);
		gf_dbg_len(gf_dev->gBuffer, 4);
	}

	return 0;
}

static int gf_check_irq(struct gf_dev *gf_dev)
{
	int i, j;
	int gpio_status_prev = 0;
	int gpio_status = 0;
	int result = -GF_PERM_ERROR;

	for (i = 0 ; i < MAX_REST_RETRY_COUNT ; i++) {
		gpio_status_prev = gpio_get_value(gf_dev->irq_gpio);
		gf_dbg("irq_gpio_prev status [%d]", gpio_status_prev);

		gf_spi_write_word(gf_dev, GF_IRQ_CTRL1, 0xFFFF); // GF_IRQ_CTRL1 set irq high time
		gf_spi_write_word(gf_dev, GF_IRQ_CTRL4, 0x0002);
		gpio_status = gpio_get_value(gf_dev->irq_gpio);

		if (gpio_status_prev == 0 && gpio_status == 0) {
			gf_dbg("start irq pin check.... irq_gpio status[%d]", gpio_status);
			for (j = 0 ; j < MAX_IRQ_PIN_CHECK_RETRY_COUNT ; j++) {
				mdelay(10);
				gpio_status = gpio_get_value(gf_dev->irq_gpio);
				if (gpio_status == 1) {
					gf_dbg("irq_gpio status high[%d] %d round", gpio_status, j);
					result = GF_NO_ERROR;
				}
			}
			if (gpio_status_prev == 0 && gpio_status == 1) {
				gf_dbg("[pass] finish irq_gpio check");
				result = GF_NO_ERROR;
			} else {
				gf_dbg("reset and retry irq_gpio check");
				gf_hw_reset(gf_dev, 5);
				continue;
			}
		} else if (gpio_status_prev == 0 && gpio_status == 1) {
			gf_dbg("irq_gpio status high [%d] w/o delay\n", gpio_status);
			gf_dbg("[pass] finish irq_gpio check");
			result = GF_NO_ERROR;
		} else {
			gf_dbg("irq_gpio_prev status was high irq_gpio_status[%d]", gpio_status);
			gf_hw_reset(gf_dev, 5);
			continue;
		}
	}

	gf_spi_write_word(gf_dev, GF_IRQ_CTRL2, 0xFFFF); // GF_IRQ_CTRL2 set irq clear
	gpio_status = gpio_get_value(gf_dev->irq_gpio);
	mdelay(10);
	if (gpio_status == 0) {
		gf_dbg("irq_gpio clear success\n");
	} else {
		gf_dbg("irq_gpio clear fail\n");
		result = -GF_PERM_ERROR;
	}

	return result;
}

void gf_spi_setup(struct gf_dev *gf_dev, enum gf_spi_transfer_speed speed)
{
	int ret = 0;

	if (speed == GF_SPI_KEEP_SPEED)
		return;
	gf_dev->spi->chip_select = 0;
	gf_dev->spi->mode = SPI_MODE_0; //CPOL=CPHA=0
	gf_dev->spi->max_speed_hz = gf_spi_speed[speed];
	gf_dev->spi->bits_per_word = 8;
	ret = spi_setup(gf_dev->spi);
	gf_dbg("%s spi_setup ret = %d", __func__, ret);
}

static int gf_setup_spi_context(struct gf_dev *gf_dev, struct spi_device *spi)
{
	gf_spi_setup(gf_dev, GF_SPI_HIGH_SPEED);
	spi_set_drvdata(spi, gf_dev);
	return GF_NO_ERROR;
}

static int gf_run_oem_test(struct gf_dev *gf_dev)
{
	int ret = GF_NO_ERROR;

	ret = gf_get_hw_id(gf_dev);
	if (ret) {
		gf_dbg("%s gf_get_hw_id failed,probe failed!\n", __func__);
		return -GF_PERM_ERROR;
	}

	ret = gf_check_irq(gf_dev);
	if (ret) {
		gf_dbg("%s gf_check_irq failed,probe failed!\n", __func__);
		return -GF_PERM_ERROR;
	}

	return GF_NO_ERROR;
}

#endif

static long gf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct gf_dev *gf_dev = &gf;
	struct gf_key gf_key = { 0 };
	int retval = 0;

	FUNC_ENTRY();

	if (_IOC_TYPE(cmd) != GF_IOC_MAGIC)
		return -ENODEV;

	if (_IOC_DIR(cmd) & _IOC_READ)
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if ((retval == 0) && (_IOC_DIR(cmd) & _IOC_WRITE))
		retval = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (retval)
		return -EFAULT;

	if (gf_dev->device_available == GF_DEVICE_NOT_AVAILABLE) {
		if ((cmd == GF_IOC_POWER_ON) || (cmd == GF_IOC_POWER_OFF)) {
			gf_dbg("power cmd\n");
		} else {
			gf_dbg("Sensor is power off currently.\n");
			return -ENODEV;
		}
	}

	switch (cmd) {
	case GF_IOC_DISABLE_IRQ:
		gf_dbg("GF_IOC_DISABLE_IRQ");
		gf_disable_irq(gf_dev);
		break;
	case GF_IOC_ENABLE_IRQ:
		gf_dbg("GF_IOC_ENABLE_IRQ");
		gf_enable_irq(gf_dev);
		break;

	case GF_IOC_RESET:
		gf_dbg("GF_IOC_RESET");
		gf_hw_reset(gf_dev, 1);
		break;

	case GF_IOC_SENDKEY:
		gf_dbg("GF_IOC_SENDKEY");
		if (copy_from_user
				(&gf_key, (struct gf_key *)arg, sizeof(struct gf_key))) {
			gf_dbg("Failed to copy data from user space.\n");
			retval = -EFAULT;
			break;
		}
		gf_dbg("KEY=%d, gf_key.value = %d", KEY_PROGRAM, gf_key.value);
		input_report_key(gf_dev->input, KEY_PROGRAM, gf_key.value);
		input_sync(gf_dev->input);
		break;

	case GF_IOC_POWER_ON:
		gf_dbg("GF_IOC_POWER_ON");
		if (gf_dev->device_available == GF_DEVICE_AVAILABLE)
			gf_dbg("Sensor has already powered-on.\n");
		else
			gf_power_on(gf_dev);
		gf_dev->device_available = GF_DEVICE_AVAILABLE;
		break;

	case GF_IOC_POWER_OFF:
		gf_dbg("GF_IOC_POWER_OFF");
		if (gf_dev->device_available == GF_DEVICE_NOT_AVAILABLE)
			gf_dbg("Sensor has already powered-off.\n");
		else
			gf_power_off(gf_dev);
		gf_dev->device_available = GF_DEVICE_NOT_AVAILABLE;
		break;

	default:
		gf_dbg("Unsupport cmd:0x%x\n", cmd);
		break;
	}

	FUNC_EXIT();

	return retval;
}

static irqreturn_t gf_irq(int irq, void *handle)
{
	/* TBD */
	gf_dbg("%s\n", __func__);

	return IRQ_HANDLED;
}

static int gf_open(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = -ENXIO;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);

	list_for_each_entry(gf_dev, &device_list, device_entry) {
		if (gf_dev->devt == inode->i_rdev) {
			gf_dbg("Found\n");
			status = 0;
			break;
		}
	}

	if (status == 0) {
		if (status == 0) {
			gf_dev->users++;
			filp->private_data = gf_dev;
			nonseekable_open(inode, filp);
			gf_dbg("Succeed to open device. irq = %d\n",
					gf_dev->irq);
			if (gf_dev->users == 1)
				gf_enable_irq(gf_dev);

			gf_power_on(gf_dev);
			gf_hw_reset(gf_dev, 5);
			//add by jicai 20161019  disable_irq to solve  always interrupt
			gf_disable_irq(gf_dev);
			gf_dev->device_available = GF_DEVICE_AVAILABLE;
		}
	} else {
		gf_dbg("No device for minor %d\n", iminor(inode));
	}
	mutex_unlock(&device_list_lock);
	FUNC_EXIT();
	return status;
}

#ifdef GF_FASYNC
static int gf_fasync(int fd, struct file *filp, int mode)
{
	struct gf_dev *gf_dev = filp->private_data;
	int ret;

	FUNC_ENTRY();
	ret = fasync_helper(fd, filp, mode, &gf_dev->async);
	FUNC_EXIT();
	gf_dbg("ret = %d\n", ret);
	return ret;
}
#endif

static int gf_release(struct inode *inode, struct file *filp)
{
	struct gf_dev *gf_dev;
	int status = 0;

	FUNC_ENTRY();
	mutex_lock(&device_list_lock);
	gf_dev = filp->private_data;
	filp->private_data = NULL;

	/*last close?? */
	gf_dev->users--;
	if (!gf_dev->users) {

		gf_dbg("disble_irq. irq = %d\n", gf_dev->irq);
		gf_disable_irq(gf_dev);

		gf_dev->device_available = GF_DEVICE_NOT_AVAILABLE;
		gf_power_off(gf_dev);
	}
	mutex_unlock(&device_list_lock);
	FUNC_EXIT();
	return status;
}

static const struct file_operations gf_fops = {
	.owner = THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	 * gets more complete API coverage.  It'll simplify things
	 * too, except for the locking.
	 */
	.unlocked_ioctl = gf_ioctl,
	.open = gf_open,
	.release = gf_release,
};

static void gf_reg_key_kernel(struct gf_dev *gf_dev)
{
	set_bit(EV_KEY, gf_dev->input->evbit);
	set_bit(KEY_PROGRAM, gf_dev->input->keybit);
}

/* SysNode */
static ssize_t gf_hw_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gf_dev *gf_dev = dev_get_drvdata(dev);
	int ret;
	ssize_t len = 0;

	len += sprintf(buf + len, "ic:%s\n", GF_SPIDEV_NAME);
	ret = gf_get_hw_id(gf_dev);
	if (ret == GF_NO_ERROR) {
		len += sprintf(buf + len, "hw:%02X%02X%02X%02X\n",
			gf_dev->gBuffer[0], gf_dev->gBuffer[1],
			gf_dev->gBuffer[2], gf_dev->gBuffer[3]);
		len += sprintf(buf + len, "spi(r/w):ok\n");
	} else {
		len += sprintf(buf + len, "spi(r/w):fail\n");
	}

	ret = gf_check_irq(gf_dev);
	if (ret == GF_NO_ERROR)
		len += sprintf(buf + len, "int:ok\n");
	else
		len += sprintf(buf + len, "int:fail\n");

	return len;
}

static DEVICE_ATTR_RO(gf_hw_id);

static ssize_t qup_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct gf_dev *gf_dev = dev_get_drvdata(dev);
// just for debug, for sdm845
	gf_dev->qup_id = 0;
	return sprintf(buf, "%d\n", gf_dev->qup_id);
}

static DEVICE_ATTR_RO(qup_id);

/* -------------------------------------------------------------------- */
static ssize_t gf_spi_prepare_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	struct gf_dev *gf_dev = dev_get_drvdata(dev);
	int res = 0;
	bool to_tz;

	if (*buf == '1')
		to_tz = true;
	else if (*buf == '0')
		to_tz = false;
	else
		return -EINVAL;

	/* set spi ownership flag */
	gf_dev->pipe_owner = to_tz;

	return res ? res : count;
}

static ssize_t gf_spi_prepare_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct gf_dev *gf_dev = dev_get_drvdata(dev);

	if (gf_dev->pipe_owner)
		return sprintf(buf, "%d\n", TZBSP_TZ_ID);
	else
		return sprintf(buf, "%d\n", TZBSP_APSS_ID);
}

static DEVICE_ATTR_RW(gf_spi_prepare);


static struct attribute *gf_attributes[] = {
	&dev_attr_gf_hw_id.attr,
	&dev_attr_qup_id.attr,
	&dev_attr_gf_spi_prepare.attr,
	NULL
};

static const struct attribute_group gf_attr_group = {
	.attrs = gf_attributes,
};


static int gf_initialize_device_data(struct gf_dev *gf_dev, struct spi_device *spi)
{
	/* Initialize the driver data */
	INIT_LIST_HEAD(&gf_dev->device_entry);
	gf_dev->spi = spi;
	gf_dev->irq_gpio = -EINVAL;
	gf_dev->reset_gpio = -EINVAL;
	gf_dev->pwr_gpio = -EINVAL;
	gf_dev->device_available = GF_DEVICE_NOT_AVAILABLE;
	gf_dev->fb_black = 0;

	mutex_init(&gf_dev->buf_lock);
	mutex_init(&gf_dev->frame_lock);
	spin_lock_init(&gf_dev->spi_lock);
	device_init_wakeup(&gf_dev->spi->dev, true);

	return GF_NO_ERROR;
}

static int gf_setup_input_device(struct gf_dev *gf_dev)
{
	/*input device subsystem */
	gf_dev->input = input_allocate_device();
	if (gf_dev->input == NULL) {
		gf_dbg("Faile to allocate input device.\n");
		return -ENOMEM;
	}

	gf_dev->input->name = GF_INPUT_NAME;

	input_set_drvdata(gf_dev->input, gf_dev);

	if (input_register_device(gf_dev->input)) {
		gf_dbg("Failed to register GF as input device.\n");
		input_free_device(gf_dev->input);
		return -ENOMEM;
	}

	return GF_NO_ERROR;
}

static int gf_setup_irq_pin(struct gf_dev *gf_dev)
{
	int ret;

	gf_dev->irq = gf_irq_num(gf_dev);
	gf_dbg("gf_dev->irq: %d\n", gf_dev->irq);
	ret = request_threaded_irq(gf_dev->irq, NULL, gf_irq,
			IRQF_TRIGGER_RISING | IRQF_ONESHOT,
			"gf", gf_dev);
	if (!ret) {
		gf_dbg("%s called enable_irq_wake.\n", __func__);
		disable_irq(gf_dev->irq);
		enable_irq(gf_dev->irq);

		gf_dev->irq_enabled = 1;
		enable_irq_wake(gf_dev->irq);
		gf_disable_irq(gf_dev);
	}

	return ret;
}

static int gf_make_device_node(struct gf_dev *gf_dev)
{
	int status = GF_NO_ERROR;
	unsigned long minor;

	/* If we can allocate a minor number, hook up this device.
	 * Reusing minors is fine so long as udev or mdev is working.
	 */
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		gf_dev->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(gf_class, &gf_dev->spi->dev, gf_dev->devt,
				gf_dev, GF_DEV_NAME);
		status = IS_ERR(dev) ? PTR_ERR(dev) : GF_NO_ERROR;
	} else {
		dev_dbg(&gf_dev->spi->dev, "no minor number available!\n");
		status = -ENODEV;
	}

	if (status == GF_NO_ERROR) {
		set_bit(minor, minors);
		list_add(&gf_dev->device_entry, &device_list);
	} else {
		gf_dev->devt = 0;
	}
	mutex_unlock(&device_list_lock);

	return status;

}

static int gf_get_alloc_gbuffer(struct gf_dev *gf_dev)
{
	gf_dev->gBuffer = kzalloc(bufsiz + GF_RDATA_OFFSET, GFP_KERNEL);
	if (gf_dev->gBuffer == NULL)
		return -ENOMEM;

	return GF_NO_ERROR;
}

static int gf_setup_lge_input_device(struct gf_dev *gf_dev)
{
	int status = GF_NO_ERROR;

	/* register input device */
	gf_dev->lge_input = input_allocate_device();
	if (!gf_dev->lge_input) {
		gf_dbg("[ERROR] input_allocate_device failed.\n");
		return -ENOMEM;
	}

	gf_dev->lge_input->name = "fingerprint";
	gf_dev->lge_input->dev.init_name = "lge_fingerprint";

	input_set_drvdata(gf_dev->lge_input, gf_dev);
	status = input_register_device(gf_dev->lge_input);
	if (status) {
		gf_dbg("[ERROR] nput_register_device failed.\n");
		input_free_device(gf_dev->lge_input);
		return -ENOMEM;
	}
	if (sysfs_create_group(&gf_dev->lge_input->dev.kobj, &gf_attr_group)) {
		gf_dbg("[ERROR] sysfs_create_group failed.\n");
		input_unregister_device(gf_dev->lge_input);
		return -ENOMEM;
	}
	return status;
}

static int gf_probe(struct spi_device *spi)
{
	struct gf_dev *gf_dev = &gf;

	FUNC_ENTRY();
	gf_dbg("GF Probe ver:%s\n", GF_DRIVER_VER);
	if (gf_initialize_device_data(gf_dev, spi))
		goto error;

	if (gf_parse_dts(gf_dev)) {
		gf_dbg("gf_parse_dts error!");
		goto error_parse_dts;
	}

	if (gf_make_device_node(gf_dev))
		goto error_make_device_node;

	if (gf_get_alloc_gbuffer(gf_dev))
		goto error_get_alloc_gbuffer;

	if (gf_setup_input_device(gf_dev))
		goto error_setup_input_device;

	if (gf_setup_lge_input_device(gf_dev))
		goto error_setup_lge_input_device;

	gf_reg_key_kernel(gf_dev);

#if DEVICE_SPI_TEST
	if (gf_setup_spi_context(gf_dev, spi))
		goto error_setup_spi_context;
#endif

	if (gf_setup_irq_pin(gf_dev))
		goto error_setup_irq_pin;

	/* power sequence */
	gpio_direction_output(gf_dev->pwr_gpio, 1);
	msleep_interruptible(10);
	gf_hw_reset(gf_dev, 0);

#if DEVICE_SPI_TEST
	/* Test code */
	/* check spi interface to FP */
	gf_run_oem_test(gf_dev);
	gf_spi_write_word(gf_dev, GF_IRQ_CTRL2, 0xFFFF);
#endif

	FUNC_EXIT();

	return GF_NO_ERROR;

error_setup_irq_pin:

#if DEVICE_SPI_TEST
error_setup_spi_context:
	if (gf_dev->lge_input != NULL) {
		sysfs_remove_group(&gf_dev->lge_input->dev.kobj, &gf_attr_group);
		input_unregister_device(gf_dev->lge_input);
	}
#endif
error_setup_lge_input_device:
	if (gf_dev->input != NULL)
		input_unregister_device(gf_dev->input);

error_setup_input_device:
	if (gf_dev->gBuffer != NULL)
		kfree(gf_dev->gBuffer);

error_get_alloc_gbuffer:
	gf_dbg("device_destroy....");
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);
	mutex_unlock(&device_list_lock);
error_make_device_node:
	gf_cleanup(gf_dev);
error_parse_dts:
error:
	gf_dev->device_available = GF_DEVICE_NOT_AVAILABLE;
	gf_probe_fail = 1;
	FUNC_EXIT();
	return -GF_PERM_ERROR;
}


static int gf_remove(struct spi_device *spi)
{
	struct gf_dev *gf_dev = &gf;

	FUNC_ENTRY();

	/* make sure ops on existing fds can abort cleanly */
	if (gf_dev->irq)
		free_irq(gf_dev->irq, gf_dev);

	if (gf_dev->input != NULL)
		input_unregister_device(gf_dev->input);
	input_free_device(gf_dev->input);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	list_del(&gf_dev->device_entry);
	device_destroy(gf_class, gf_dev->devt);
	clear_bit(MINOR(gf_dev->devt), minors);
	if (gf_dev->users == 0)
		gf_cleanup(gf_dev);
	gf_dev->device_available = GF_DEVICE_NOT_AVAILABLE;

	if (gf_dev->users == 0)
		kfree(gf_dev->gBuffer);
	else
		gf_dbg("Not free_pages.\n");

	mutex_unlock(&device_list_lock);

	FUNC_EXIT();
	return 0;
}

static const struct of_device_id gx_match_table[] = {
	{.compatible = GF_SPIDEV_NAME},
	{}
};

static struct spi_driver gf_driver = {

	.driver = {
		.name = GF_DEV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gx_match_table,
	},
	.probe = gf_probe,
	.remove = gf_remove,

};

static int __init gf_init(void)
{
	int status;

	FUNC_ENTRY();
	/* Claim our 256 reserved device numbers.  Then register a class
	 * that will key udev/mdev to add/remove /dev nodes.  Last, register
	 * the driver which manages those device numbers.
	 */
	BUILD_BUG_ON(N_SPI_MINORS > 256);
	status = register_chrdev(SPIDEV_MAJOR, CHRD_DRIVER_NAME, &gf_fops);
	if (status < 0) {
		gf_dbg("Failed to register char device!\n");
		return status;
	}

	gf_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(gf_class)) {
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		gf_dbg("Failed to create class.\n");
		return PTR_ERR(gf_class);
	}

	status = spi_register_driver(&gf_driver);

	if (status < 0) {
		class_destroy(gf_class);
		unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
		gf_dbg("Failed to register SPI driver.\n");
	}

	gf_dbg("fingerprint fod init status = 0x%x\n", status);


	FUNC_EXIT();
	return 0;
}

module_init(gf_init);

static void __exit gf_exit(void)
{
	FUNC_ENTRY();
	spi_unregister_driver(&gf_driver);
	class_destroy(gf_class);
	unregister_chrdev(SPIDEV_MAJOR, gf_driver.driver.name);
	FUNC_EXIT();
}

module_exit(gf_exit);

MODULE_AUTHOR("Jiangtao Yi, <yijiangtao@goodix.com>");
MODULE_DESCRIPTION("User mode SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi:gf-spi");
