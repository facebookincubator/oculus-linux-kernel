/*
 * STMicroelectronics st_acc33 sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/unaligned.h>

#include "st_acc33.h"

#define REG_WHOAMI_ADDR			0x0f
#define REG_WHOAMI_VAL			0x33

#define REG_OUTX_L_ADDR			0x28
#define REG_OUTY_L_ADDR			0x2a
#define REG_OUTZ_L_ADDR			0x2c

#define REG_CTRL1_ADDR			0x20
#define REG_CTRL1_ODR_MASK		0xf0

#define REG_CTRL3_ACC_ADDR		0x22
#define REG_CTRL3_ACC_I1_OVR_MASK	0x02
#define REG_CTRL3_ACC_I1_WTM_MASK	0x04
#define REG_CTRL3_ACC_I1_DRDY1_MASK	0x10

#define REG_CTRL4_ADDR			0x23
#define REG_CTRL4_BDU_MASK		0x80
#define REG_CTRL4_FS_MASK		0x30

#define REG_CTRL5_ACC_ADDR		0x24
#define REG_CTRL5_ACC_FIFO_EN_MASK	0x40
#define REG_CTRL5_ACC_BOOT_MASK		0x80

#define REG_CTRL6_ACC_ADDR		0x25

#define REG_FIFO_CTRL_REG		0x2e
#define REG_FIFO_CTRL_REG_WTM_MASK	0x1f
#define REG_FIFO_CTRL_MODE_MASK		0xc0

#define REG_FIFO_SRC_ADDR		0x2f
#define REG_FIFO_SRC_OVR_MASK		0x40
#define REG_FIFO_SRC_FSS_MASK		0x1f

#define ST_ACC33_FS_2G			IIO_G_TO_M_S_2(980)
#define ST_ACC33_FS_4G			IIO_G_TO_M_S_2(1950)
#define ST_ACC33_FS_8G			IIO_G_TO_M_S_2(3900)
#define ST_ACC33_FS_16G			IIO_G_TO_M_S_2(11720)

#define ST_ACC33_MAX_WATERMARK		31

struct st_acc33_odr {
	u32 hz;
	u8 val;
};

static const struct st_acc33_odr st_acc33_odr_table[] = {
	{   1, 0x01 },	/* 1Hz */
	{  10, 0x02 },	/* 10Hz */
	{  25, 0x03 },	/* 25Hz */
	{  50, 0x04 },	/* 50Hz */
	{ 100, 0x05 },	/* 100Hz */
	{ 200, 0x06 },	/* 200Hz */
	{ 400, 0x07 },	/* 400Hz */
};

struct st_acc33_fs {
	u32 gain;
	u8 val;
};

static const struct st_acc33_fs st_acc33_fs_table[] = {
	{  ST_ACC33_FS_2G, 0x00 },
	{  ST_ACC33_FS_4G, 0x01 },
	{  ST_ACC33_FS_8G, 0x02 },
	{ ST_ACC33_FS_16G, 0x03 },
};

const struct iio_event_spec st_acc33_fifo_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

static const struct iio_chan_spec st_acc33_channels[] = {
	ST_ACC33_DATA_CHANNEL(REG_OUTX_L_ADDR, IIO_MOD_X, 0),
	ST_ACC33_DATA_CHANNEL(REG_OUTY_L_ADDR, IIO_MOD_Y, 1),
	ST_ACC33_DATA_CHANNEL(REG_OUTZ_L_ADDR, IIO_MOD_Z, 2),
	ST_ACC33_FLUSH_CHANNEL(),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static int st_acc33_write_with_mask(struct st_acc33_dev *dev, u8 addr,
				    u8 mask, u8 val)
{
	u8 data;
	int err;

	mutex_lock(&dev->lock);

	err = dev->tf->read(dev->dev, addr, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to read %02x register\n", addr);
		mutex_unlock(&dev->lock);

		return err;
	}

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);

	err = dev->tf->write(dev->dev, addr, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to write %02x register\n", addr);
		mutex_unlock(&dev->lock);

		return err;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

static int st_acc33_update_odr(struct st_acc33_dev *dev, u16 odr)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(st_acc33_odr_table); i++) {
		if (st_acc33_odr_table[i].hz == odr)
			break;
	}

	if (i == ARRAY_SIZE(st_acc33_odr_table))
		return -EINVAL;

	err = st_acc33_write_with_mask(dev, REG_CTRL1_ADDR,
				       REG_CTRL1_ODR_MASK,
				       st_acc33_odr_table[i].val);
	if (err < 0)
		return err;

	dev->odr = odr;

	return 0;
}

int st_acc33_read_hwfifo(struct st_acc33_dev *dev, bool flush)
{
	u8 buffer[ALIGN(6, sizeof(s64)) + sizeof(s64)], data, nsamples;
	s64 ts, delta_ts;
	int i, err;

	err = dev->tf->read(dev->dev, REG_FIFO_SRC_ADDR, sizeof(data), &data);
	if (err < 0)
		return err;

	nsamples = (data & REG_FIFO_SRC_FSS_MASK);
	if (!nsamples)
		return 0;

	delta_ts = flush ? div_s64(1000000000LL, dev->odr)
			 : div_s64(dev->delta_ts, nsamples);

	for (i = 0; i < nsamples; i++) {
		err = dev->tf->read(dev->dev, REG_OUTX_L_ADDR, 6,
				    buffer);
		if (err < 0)
			return err;

		ts = dev->ts - (nsamples - 1 - i) * delta_ts;
		iio_push_to_buffers_with_timestamp(iio_priv_to_dev(dev),
						   buffer, ts);
	}

	return nsamples;
}

int st_acc33_set_enable(struct st_acc33_dev *dev, bool enable)
{
	int err;

	if (enable)
		err = st_acc33_update_odr(dev, dev->odr);
	else
		err = st_acc33_write_with_mask(dev, REG_CTRL1_ADDR,
					       REG_CTRL1_ODR_MASK, 0);

	return err < 0 ? err : 0;
}

static int st_acc33_set_fs(struct st_acc33_dev *dev, u32 gain)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(st_acc33_fs_table); i++) {
		if (st_acc33_fs_table[i].gain == gain)
			break;
	}

	if (i == ARRAY_SIZE(st_acc33_fs_table))
		return -EINVAL;

	err = st_acc33_write_with_mask(dev, REG_CTRL4_ADDR,
				       REG_CTRL4_FS_MASK,
				       st_acc33_fs_table[i].val);
	if (err < 0)
		return err;

	dev->gain = gain;

	return 0;
}

static int st_acc33_read_raw(struct iio_dev *iio_dev,
			     struct iio_chan_spec const *ch,
			     int *val, int *val2, long mask)
{
	struct st_acc33_dev *dev = iio_priv(iio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW: {
		u8 data[2];
		int err, delay;

		mutex_lock(&iio_dev->mlock);

		if (iio_buffer_enabled(iio_dev)) {
			mutex_unlock(&iio_dev->mlock);
			return -EBUSY;
		}

		err = st_acc33_set_enable(dev, true);
		if (err < 0) {
			mutex_unlock(&iio_dev->mlock);
			return err;
		}

		/* sample to discard, 3 * odr us */
		delay = 3000000 / dev->odr;
		usleep_range(delay, delay + 1);

		err = dev->tf->read(dev->dev, ch->address, 2, data);
		if (err < 0) {
			mutex_unlock(&iio_dev->mlock);
			return err;
		}

		err = st_acc33_set_enable(dev, false);
		if (err < 0) {
			mutex_unlock(&iio_dev->mlock);
			return err;
		}

		*val = (s16)get_unaligned_le16(data);
		*val = *val >> ch->scan_type.shift;

		mutex_unlock(&iio_dev->mlock);

		return IIO_VAL_INT;
	}
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = dev->gain;

		return IIO_VAL_INT_PLUS_MICRO;
	default:
		return -EINVAL;
	}

	return 0;
}

static int st_acc33_write_raw(struct iio_dev *iio_dev,
			      struct iio_chan_spec const *chan,
			      int val, int val2, long mask)
{
	int err;

	switch (mask) {
	case IIO_CHAN_INFO_SCALE: {
		mutex_lock(&iio_dev->mlock);

		if (iio_buffer_enabled(iio_dev)) {
			mutex_unlock(&iio_dev->mlock);
			return -EBUSY;
		}
		err = st_acc33_set_fs(iio_priv(iio_dev), val2);

		mutex_unlock(&iio_dev->mlock);

		break;
	}
	default:
		return -EINVAL;
	}

	return err;
}

static ssize_t st_acc33_get_sampling_frequency(struct device *device,
					       struct device_attribute *attr,
					       char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_acc33_dev *dev = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", dev->odr);
}

static ssize_t st_acc33_set_sampling_frequency(struct device *device,
					       struct device_attribute *attr,
					       const char *buf, size_t count)
{
	int err, odr;
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_acc33_dev *dev = iio_priv(iio_dev);

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	if (dev->odr == odr)
		return count;

	err = st_acc33_update_odr(dev, odr);

	return err < 0 ? err : count;
}

static ssize_t
st_acc33_get_sampling_frequency_avail(struct device *device,
				      struct device_attribute *attr,
				      char *buf)
{
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(st_acc33_odr_table); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_acc33_odr_table[i].hz);
	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_acc33_get_scale_avail(struct device *device,
					struct device_attribute *attr,
					char *buf)
{
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(st_acc33_fs_table); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_acc33_fs_table[i].gain);
	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_acc33_get_hwfifo_enabled(struct device *device,
					   struct device_attribute *attr,
					   char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);

	return sprintf(buf, "%d\n", iio_buffer_enabled(iio_dev));
}

static ssize_t st_acc33_set_hwfifo_enabled(struct device *device,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	return size;
}

static ssize_t st_acc33_get_hwfifo_watermark(struct device *device,
					     struct device_attribute *attr,
					     char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_acc33_dev *dev = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", dev->watermark);
}

static ssize_t st_acc33_set_hwfifo_watermark(struct device *device,
					     struct device_attribute *attr,
					     const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_acc33_dev *dev = iio_priv(iio_dev);
	int err, watermark;
	u8 data;

	err = kstrtoint(buf, 10, &watermark);
	if (err < 0)
		return err;

	if (watermark < 1 || watermark > ST_ACC33_MAX_WATERMARK)
		return -EINVAL;

	mutex_lock(&iio_dev->mlock);

	data = watermark ? 1 : 0;
	err = st_acc33_write_with_mask(dev, REG_CTRL5_ACC_ADDR,
				       REG_CTRL5_ACC_FIFO_EN_MASK, data);
	if (err < 0)
		goto unlock;

	err = st_acc33_write_with_mask(dev, REG_CTRL3_ACC_ADDR,
				       REG_CTRL3_ACC_I1_WTM_MASK,
				       data);
	if (err < 0)
		goto unlock;

	err = st_acc33_write_with_mask(dev, REG_FIFO_CTRL_REG,
				       REG_FIFO_CTRL_REG_WTM_MASK,
				       watermark);
	if (err < 0)
		goto unlock;

	data = watermark ? ST_ACC33_FIFO_STREAM : ST_ACC33_FIFO_BYPASS;
	err = st_acc33_write_with_mask(dev, REG_FIFO_CTRL_REG,
				       REG_FIFO_CTRL_MODE_MASK,
				       data);
	if (err < 0)
		goto unlock;

	dev->watermark = watermark;

unlock:
	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : size;
}

static ssize_t st_acc33_get_min_hwfifo_watermark(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	return sprintf(buf, "%d\n", 1);
}

static ssize_t st_acc33_get_max_hwfifo_watermark(struct device *dev,
						 struct device_attribute *attr,
						 char *buf)
{
	return sprintf(buf, "%d\n", ST_ACC33_MAX_WATERMARK);
}

ssize_t st_acc33_flush_hwfifo(struct device *device,
			      struct device_attribute *attr,
			      const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_acc33_dev *dev = iio_priv(iio_dev);
	s64 type, code;
	int err;

	mutex_lock(&iio_dev->mlock);
	if (!iio_buffer_enabled(iio_dev)) {
		mutex_unlock(&iio_dev->mlock);
		return -EINVAL;
	}

	disable_irq(dev->irq);

	err = st_acc33_read_hwfifo(dev, true);

	type = (err > 0) ? IIO_EV_DIR_FIFO_DATA : IIO_EV_DIR_FIFO_EMPTY;
	code = IIO_UNMOD_EVENT_CODE(IIO_ACCEL, -1,
				    IIO_EV_TYPE_FIFO_FLUSH, type);
	iio_push_event(iio_dev, code, dev->ts);

	enable_irq(dev->irq);

	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : size;
}

static ST_ACC33_SAMPLE_FREQ_AVAIL_ATTR();
static ST_ACC33_SCALE_AVAIL_ATTR(in_accel_scale_available);
static ST_ACC33_SAMPLE_FREQ_ATTR();
static ST_ACC33_HWFIFO_ENABLED();
static ST_ACC33_HWFIFO_WATERMARK();
static ST_ACC33_HWFIFO_WATERMARK_MIN();
static ST_ACC33_HWFIFO_WATERMARK_MAX();
static ST_ACC33_HWFIFO_FLUSH();

static struct attribute *st_acc33_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_hwfifo_enabled.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_min.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_acc33_attribute_group = {
	.attrs = st_acc33_attributes,
};

static const struct iio_info st_acc33_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_acc33_attribute_group,
	.read_raw = st_acc33_read_raw,
	.write_raw = st_acc33_write_raw,
};

static int st_acc33_check_whoami(struct st_acc33_dev *dev)
{
	u8 data;
	int err;

	err = dev->tf->read(dev->dev, REG_WHOAMI_ADDR, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != REG_WHOAMI_VAL) {
		dev_err(dev->dev, "wrong whoami {%02x-%02x}\n",
			data, REG_WHOAMI_VAL);
		return -ENODEV;
	}

	return 0;
}

int st_acc33_probe(struct st_acc33_dev *dev)
{
	int err;
	struct iio_dev *iio_dev = iio_priv_to_dev(dev);

	mutex_init(&dev->lock);

	err = st_acc33_check_whoami(dev);
	if (err < 0)
		return err;

	iio_dev->channels = st_acc33_channels;
	iio_dev->num_channels = ARRAY_SIZE(st_acc33_channels);
	iio_dev->info = &st_acc33_info;
	iio_dev->modes = INDIO_DIRECT_MODE;

	dev->odr = st_acc33_odr_table[4].hz;
	dev->watermark = ST_ACC33_MAX_WATERMARK / 2;

	err = st_acc33_set_fs(dev, ST_ACC33_FS_4G);
	if (err < 0)
		return err;

	err = st_acc33_write_with_mask(dev, REG_CTRL4_ADDR,
				       REG_CTRL4_BDU_MASK, 1);
	if (err < 0)
		return err;

	if (dev->irq > 0) {
		err = st_acc33_init_ring(dev);
		if (err < 0)
			return err;
	}

	err = devm_iio_device_register(dev->dev, iio_dev);
	if (err && dev->irq > 0)
		st_acc33_deallocate_ring(dev);

	return err;
}
EXPORT_SYMBOL(st_acc33_probe);

void st_acc33_remove(struct st_acc33_dev *dev)
{
	if (dev->irq > 0)
		st_acc33_deallocate_ring(dev);
}
EXPORT_SYMBOL(st_acc33_remove);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_acc33 sensor driver");
MODULE_LICENSE("GPL v2");
