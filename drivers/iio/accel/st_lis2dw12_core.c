/*
 * STMicroelectronics lis2dw12 driver
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
#include <linux/delay.h>
#include <linux/iio/sysfs.h>
#include <linux/interrupt.h>
#include <asm/unaligned.h>

#include "st_lis2dw12.h"

#define ST_LIS2DW12_WHOAMI_ADDR			0x0f
#define ST_LIS2DW12_WHOAMI_VAL			0x44

#define ST_LIS2DW12_CTRL1_ADDR			0x20
#define ST_LIS2DW12_ODR_MASK			0xf0
#define ST_LIS2DW12_MODE_MASK			0x0c
#define ST_LIS2DW12_LP_MODE_MASK		0x03

#define ST_LIS2DW12_CTRL2_ADDR			0x21
#define ST_LIS2DW12_BDU_MASK			0x08
#define ST_LIS2DW12_RESET_MASK			0x40

#define ST_LIS2DW12_CTRL3_ADDR			0x22
#define ST_LIS2DW12_LIR_MASK			0x10
#define ST_LIS2DW12_ST_MASK			0xc0

#define ST_LIS2DW12_CTRL4_INT1_CTRL_ADDR	0x23
#define ST_LIS2DW12_DRDY_MASK			BIT(0)
#define ST_LIS2DW12_FTH_INT1_MASK		BIT(1)
#define ST_LIS2DW12_TAP_INT1_MASK		0x48
#define ST_LIS2DW12_TAP_TAP_INT1_MASK		0x08
#define ST_LIS2DW12_FF_INT1_MASK		0x10
#define ST_LIS2DW12_WU_INT1_MASK		0x20

#define ST_LIS2DW12_CTRL6_ADDR			0x25
#define ST_LIS2DW12_LN_MASK			0x04
#define ST_LIS2DW12_FS_MASK			0x30
#define ST_LIS2DW12_BW_MASK			0xc0

#define ST_LIS2DW12_OUT_X_L_ADDR		0x28
#define ST_LIS2DW12_OUT_Y_L_ADDR		0x2a
#define ST_LIS2DW12_OUT_Z_L_ADDR		0x2c

#define ST_LIS2DW12_FIFO_CTRL_ADDR		0x2e
#define ST_LIS2DW12_FIFOMODE_MASK		0xe0
#define ST_LIS2DW12_FTH_MASK			0x1f

#define ST_LIS2DW12_TAP_THS_X_ADDR		0x30
#define ST_LIS2DW12_TAP_THS_Y_ADDR		0x31
#define ST_LIS2DW12_TAP_THS_Z_ADDR		0x32

#define ST_LIS2DW12_TAP_AXIS_MASK		0xe0
#define ST_LIS2DW12_TAP_THS_MAK			0x1f

#define ST_LIS2DW12_INT_DUR_ADDR		0x33

#define ST_LIS2DW12_WAKE_UP_THS_ADDR		0x34
#define ST_LIS2DW12_WAKE_UP_THS_MAK		0x3f
#define ST_LIS2DW12_SINGLE_DOUBLE_TAP_MAK	0x80

#define ST_LIS2DW12_FREE_FALL_ADDR		0x36
#define ST_LIS2DW12_FREE_FALL_THS_MASK		0x07
#define ST_LIS2DW12_FREE_FALL_DUR_MASK		0xf8

#define ST_LIS2DW12_ABS_INT_CFG_ADDR		0x3f
#define ST_LIS2DW12_ALL_INT_MASK		0x20
#define ST_LIS2DW12_INT2_ON_INT1_MASK		0x40
#define ST_LIS2DW12_DRDY_PULSED_MASK		0x80

#define ST_LIS2DW12_FS_2G_GAIN			IIO_G_TO_M_S_2(244)
#define ST_LIS2DW12_FS_4G_GAIN			IIO_G_TO_M_S_2(488)
#define ST_LIS2DW12_FS_8G_GAIN			IIO_G_TO_M_S_2(976)
#define ST_LIS2DW12_FS_16G_GAIN			IIO_G_TO_M_S_2(1952)

#define ST_LIS2DW12_SELFTEST_MIN		285
#define ST_LIS2DW12_SELFTEST_MAX		6150

struct st_lis2dw12_odr {
	u16 hz;
	u8 val;
};

static const struct st_lis2dw12_odr st_lis2dw12_odr_table[] = {
	{    0, 0x0 }, /* power-down */
	{    1, 0x1 }, /* LP 1.6Hz */
	{   12, 0x2 }, /* LP 12.5Hz */
	{   25, 0x3 }, /* LP 25Hz*/
	{   50, 0x4 }, /* LP 50Hz*/
	{  100, 0x5 }, /* LP 100Hz*/
	{  200, 0x6 }, /* LP 200Hz*/
	{  400, 0x7 }, /* HP 400Hz*/
	{  800, 0x8 }, /* HP 800Hz*/
	{ 1600, 0x9 }, /* HP 1600Hz*/
};

struct st_lis2dw12_fs {
	u32 gain;
	u8 val;
};

static const struct st_lis2dw12_fs st_lis2dw12_fs_table[] = {
	{  ST_LIS2DW12_FS_2G_GAIN, 0x0 },
	{  ST_LIS2DW12_FS_4G_GAIN, 0x1 },
	{  ST_LIS2DW12_FS_8G_GAIN, 0x2 },
	{ ST_LIS2DW12_FS_16G_GAIN, 0x3 },
};

struct st_lis2dw12_selftest_req {
	char *mode;
	u8 val;
};

struct st_lis2dw12_selftest_req st_lis2dw12_selftest_table[] = {
	{ "disabled", 0x0 },
	{ "positive-sign", 0x1 },
	{ "negative-sign", 0x2 },
};

#define ST_LIS2DW12_ACC_CHAN(addr, ch2, idx)			\
{								\
	.type = IIO_ACCEL,					\
	.address = addr,					\
	.modified = 1,						\
	.channel2 = ch2,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_index = idx,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 14,					\
		.storagebits = 16,				\
		.shift = 2,					\
		.endianness = IIO_LE,				\
	},							\
}

const struct iio_event_spec st_lis2dw12_fifo_flush_event = {
	.type = IIO_EV_TYPE_FIFO_FLUSH,
	.dir = IIO_EV_DIR_EITHER,
};

const struct iio_event_spec st_lis2dw12_rthr_event = {
	.type = IIO_EV_TYPE_THRESH,
	.dir = IIO_EV_DIR_RISING,
};

#define ST_LIS2DW12_EVENT_CHANNEL(chan_type, evt_spec)	\
{							\
	.type = chan_type,				\
	.scan_index = -1,				\
	.indexed = -1,					\
	.event_spec = evt_spec,				\
	.num_event_specs = 1,				\
}

static const struct iio_chan_spec st_lis2dw12_acc_channels[] = {
	ST_LIS2DW12_ACC_CHAN(ST_LIS2DW12_OUT_X_L_ADDR, IIO_MOD_X, 0),
	ST_LIS2DW12_ACC_CHAN(ST_LIS2DW12_OUT_Y_L_ADDR, IIO_MOD_Y, 1),
	ST_LIS2DW12_ACC_CHAN(ST_LIS2DW12_OUT_Z_L_ADDR, IIO_MOD_Z, 2),
	ST_LIS2DW12_EVENT_CHANNEL(IIO_ACCEL, &st_lis2dw12_fifo_flush_event),
	ST_LIS2DW12_EVENT_CHANNEL(IIO_TAP, &st_lis2dw12_rthr_event),
	ST_LIS2DW12_EVENT_CHANNEL(IIO_TAP_TAP, &st_lis2dw12_rthr_event),
	ST_LIS2DW12_EVENT_CHANNEL(IIO_TILT, &st_lis2dw12_rthr_event),
	IIO_CHAN_SOFT_TIMESTAMP(3),
};

static int st_lis2dw12_write_with_mask(struct st_lis2dw12_hw *hw, u8 addr,
				       u8 mask, u8 val)
{
	u8 data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto out;
	}

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

out:
	mutex_unlock(&hw->lock);

	return err;
}

static int st_lis2dw12_set_fs(struct st_lis2dw12_hw *hw, u16 gain)
{
	int i, err;

	for (i = 0; i < ARRAY_SIZE(st_lis2dw12_fs_table); i++)
		if (st_lis2dw12_fs_table[i].gain == gain)
			break;

	if (i == ARRAY_SIZE(st_lis2dw12_fs_table))
		return -EINVAL;

	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL6_ADDR,
					  ST_LIS2DW12_FS_MASK,
					  st_lis2dw12_fs_table[i].val);
	if (err < 0)
		return err;

	hw->gain = gain;

	return 0;
}

static int st_lis2dw12_set_odr(struct st_lis2dw12_hw *hw, u16 odr)
{
	u8 mode, val;
	int i, err;

	for (i = 0; i < ARRAY_SIZE(st_lis2dw12_odr_table); i++)
		if (st_lis2dw12_odr_table[i].hz == odr)
			break;

	if (i == ARRAY_SIZE(st_lis2dw12_odr_table))
		return -EINVAL;

	if (hw->event_mask && odr < st_lis2dw12_odr_table[7].hz)
		return 0;

	mode = odr > 200 ? 0x1 : 0x0;
	val = (st_lis2dw12_odr_table[i].val << __ffs(ST_LIS2DW12_ODR_MASK)) |
	      (mode << __ffs(ST_LIS2DW12_MODE_MASK)) | 0x01;

	err = hw->tf->write(hw->dev, ST_LIS2DW12_CTRL1_ADDR, sizeof(val),
			    &val);

	return err < 0 ?  err : 0;
}

static int st_lis2dw12_check_whoami(struct st_lis2dw12_hw *hw)
{
	int err;
	u8 data;

	err = hw->tf->read(hw->dev, ST_LIS2DW12_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_LIS2DW12_WHOAMI_VAL) {
		dev_err(hw->dev, "wrong whoami %02x vs %02x\n",
			data, ST_LIS2DW12_WHOAMI_VAL);
		return -ENODEV;
	}

	return 0;
}

int st_lis2dw12_set_fifomode(struct st_lis2dw12_hw *hw,
			     enum st_lis2dw12_fifo_mode mode)
{
	u8 drdy_val = !!mode;
	int err;

	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL4_INT1_CTRL_ADDR,
					  ST_LIS2DW12_FTH_INT1_MASK, drdy_val);
	if (err < 0)
		return err;

	return st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_FIFO_CTRL_ADDR,
					   ST_LIS2DW12_FIFOMODE_MASK, mode);
}

static int st_lis2dw12_init_hw(struct st_lis2dw12_hw *hw)
{
	int err;

	/* soft reset the device */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL2_ADDR,
					  ST_LIS2DW12_RESET_MASK, 1);
	if (err < 0)
		return err;

	/* enable latched interrupt */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL3_ADDR,
					  ST_LIS2DW12_LIR_MASK, 1);
	if (err < 0)
		return err;

	/* enable bdu */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL2_ADDR,
					  ST_LIS2DW12_BDU_MASK, 1);
	if (err < 0)
		return err;

	/* route interrupt from INT2 to INT1 pin */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_ABS_INT_CFG_ADDR,
					  ST_LIS2DW12_INT2_ON_INT1_MASK, 1);
	if (err < 0)
		return err;

	/* enable all interrupts */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_ABS_INT_CFG_ADDR,
					  ST_LIS2DW12_ALL_INT_MASK, 1);
	if (err < 0)
		return err;

	/* configure fifo watermark */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_FIFO_CTRL_ADDR,
					  ST_LIS2DW12_FTH_MASK, hw->watermark);
	if (err < 0)
		return err;

	/* configure default free fall event threshold */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_FREE_FALL_ADDR,
					  ST_LIS2DW12_FREE_FALL_THS_MASK, 1);
	if (err < 0)
		return err;

	/* configure default free fall event duration */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_FREE_FALL_ADDR,
					  ST_LIS2DW12_FREE_FALL_DUR_MASK, 1);
	if (err < 0)
		return err;

	/* enable tap event on all axes */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_TAP_THS_Z_ADDR,
					  ST_LIS2DW12_TAP_AXIS_MASK, 0x7);
	if (err < 0)
		return err;

	/* configure default threshold for Tap event recognition */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_TAP_THS_X_ADDR,
					  ST_LIS2DW12_TAP_THS_MAK, 9);
	if (err < 0)
		return err;

	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_TAP_THS_Y_ADDR,
					  ST_LIS2DW12_TAP_THS_MAK, 9);
	if (err < 0)
		return err;

	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_TAP_THS_Z_ADDR,
					  ST_LIS2DW12_TAP_THS_MAK, 9);
	if (err < 0)
		return err;

	/* Low Noise enabled by default */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL6_ADDR,
					  ST_LIS2DW12_LN_MASK, 1);
	if (err < 0)
		return err;

	/* BW = ODR/4 */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL6_ADDR,
					  ST_LIS2DW12_BW_MASK, 1);

	return err < 0 ? err : 0;
}

static ssize_t
st_lis2dw12_sysfs_sampling_frequency_avl(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(st_lis2dw12_odr_table); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 st_lis2dw12_odr_table[i].hz);
	buf[len - 1] = '\n';

	return len;
}

static ssize_t st_lis2dw12_sysfs_scale_avail(struct device *dev,
					     struct device_attribute *attr,
					     char *buf)
{
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(st_lis2dw12_fs_table); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "0.%06u ",
				 st_lis2dw12_fs_table[i].gain);
	buf[len - 1] = '\n';

	return len;
}

int st_lis2dw12_sensor_set_enable(struct st_lis2dw12_hw *hw, bool enable)
{
	u16 val = enable ? hw->odr : 0;
	int err;

	err = st_lis2dw12_set_odr(hw, val);

	return err < 0 ? err : 0;
}

static int st_lis2dw12_read_oneshot(struct st_lis2dw12_hw *hw,
				    u8 addr, int *val)
{
	int err, delay;
	u8 data[2];

	err = st_lis2dw12_sensor_set_enable(hw, true);
	if (err < 0)
		return err;

	/* sample to discard, 3 * odr us */
	delay = 3000000 / hw->odr;
	usleep_range(delay, delay + 1);

	err = hw->tf->read(hw->dev, addr, sizeof(data), data);
	if (err < 0)
		return err;

	st_lis2dw12_sensor_set_enable(hw, false);

	*val = (s16)get_unaligned_le16(data);

	return IIO_VAL_INT;
}

static int st_lis2dw12_read_raw(struct iio_dev *iio_dev,
				struct iio_chan_spec const *ch,
				int *val, int *val2, long mask)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int ret;

	mutex_lock(&iio_dev->mlock);

	if (iio_buffer_enabled(iio_dev)) {
		mutex_unlock(&iio_dev->mlock);
		return -EBUSY;
	}

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = st_lis2dw12_read_oneshot(hw, ch->address, val);
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = hw->gain;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return ret;
}

static int st_lis2dw12_write_raw(struct iio_dev *iio_dev,
				 struct iio_chan_spec const *chan,
				 int val, int val2, long mask)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err;

	mutex_lock(&iio_dev->mlock);

	if (iio_buffer_enabled(iio_dev)) {
		mutex_unlock(&iio_dev->mlock);
		return -EBUSY;
	}

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		err = st_lis2dw12_set_fs(hw, val2);
		break;
	default:
		err = -EINVAL;
		break;
	}

	mutex_unlock(&iio_dev->mlock);

	return err;
}

static int st_lis2dw12_write_event_config(struct iio_dev *iio_dev,
					  const struct iio_chan_spec *chan,
					  enum iio_event_type type,
					  enum iio_event_direction dir,
					  int state)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	u8 data[2] = {}, drdy_val, drdy_mask;
	enum st_lis2dw12_event_id id;
	int err, odr;

	switch (chan->type) {
	case IIO_TAP:
		id = ST_LIS2DW12_EVT_TAP;
		data[0] = state ? 6 : 0;
		drdy_val = state ? 9 : 0;
		drdy_mask = ST_LIS2DW12_TAP_INT1_MASK;
		break;
	case IIO_TAP_TAP:
		id = ST_LIS2DW12_EVT_TAP_TAP;
		data[0] = state ? 0x7f : 0;
		data[1] = state ? 0x80 : 0;
		drdy_val = state ? 1 : 0;
		drdy_mask = ST_LIS2DW12_TAP_TAP_INT1_MASK;
		break;
	case IIO_TILT:
		id = ST_LIS2DW12_EVT_WU;
		data[1] = state ? 0x02 : 0;
		drdy_val = state ? 1 : 0;
		drdy_mask = ST_LIS2DW12_WU_INT1_MASK;
		break;
	default:
		return -EINVAL;
	};

	err = hw->tf->write(hw->dev, ST_LIS2DW12_INT_DUR_ADDR,
			    sizeof(data), data);
	if (err < 0)
		return err;

	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL4_INT1_CTRL_ADDR,
					  drdy_mask, drdy_val);
	if (err < 0)
		return err;

	odr = state ? st_lis2dw12_odr_table[7].hz : hw->odr;
	err = st_lis2dw12_set_odr(hw, odr);
	if (err < 0)
		return err;

	if (state)
		hw->event_mask |= BIT(id);
	else
		hw->event_mask &= ~BIT(id);

	return 0;
}

static ssize_t st_lis2dw12_get_hwfifo_watermark(struct device *dev,
						struct device_attribute *attr,
						char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", hw->watermark);
}

static ssize_t st_lis2dw12_set_hwfifo_watermark(struct device *device,
						struct device_attribute *attr,
						const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(device);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err = 0, watermark;

	err = kstrtoint(buf, 10, &watermark);
	if (err < 0)
		return err;

	if (watermark < 1 || watermark > ST_LIS2DW12_MAX_WATERMARK)
		return -EINVAL;

	mutex_lock(&iio_dev->mlock);
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_FIFO_CTRL_ADDR,
					  ST_LIS2DW12_FTH_MASK, watermark);
	mutex_unlock(&iio_dev->mlock);

	if (err < 0)
		return err;

	hw->watermark = watermark;

	return size;
}

static ssize_t
st_lis2dw12_get_min_hwfifo_watermark(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "%d\n", 1);
}

static ssize_t
st_lis2dw12_get_max_hwfifo_watermark(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	return sprintf(buf, "%d\n", ST_LIS2DW12_MAX_WATERMARK);
}

static ssize_t st_lis2dw12_get_sampling_frequency(struct device *dev,
						  struct device_attribute *attr,
						  char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);

	return sprintf(buf, "%d\n", hw->odr);
}

static ssize_t st_lis2dw12_set_sampling_frequency(struct device *dev,
						  struct device_attribute *attr,
						  const char *buf, size_t count)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err, odr;

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	mutex_lock(&iio_dev->mlock);

	err = st_lis2dw12_set_odr(hw, odr);
	if (!err)
		hw->odr = odr;

	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : count;
}

static ssize_t st_lis2dw12_get_hwfifo_enabled(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", iio_buffer_enabled(iio_dev));
}

static ssize_t st_lis2dw12_set_hwfifo_enabled(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	return size;
}

static ssize_t st_lis2dw12_flush_fifo(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	s64 type, code;
	int samples;

	mutex_lock(&iio_dev->mlock);
	if (!iio_buffer_enabled(iio_dev)) {
		mutex_unlock(&iio_dev->mlock);
		return -EINVAL;
	}
	disable_irq(hw->irq);

	samples = st_lis2dw12_read_fifo(hw, true);
	type = samples > 0 ? IIO_EV_DIR_FIFO_DATA : IIO_EV_DIR_FIFO_EMPTY;
	code = IIO_UNMOD_EVENT_CODE(IIO_ACCEL, -1,
				    IIO_EV_TYPE_FIFO_FLUSH, type);

	iio_push_event(iio_dev, code, hw->ts);

	enable_irq(hw->irq);
	mutex_unlock(&iio_dev->mlock);

	return samples < 0 ? samples : size;
}

static ssize_t st_lis2dw12_get_selftest_avail(struct device *dev,
					      struct device_attribute *attr,
					      char *buf)
{
	return sprintf(buf, "%s, %s\n", st_lis2dw12_selftest_table[1].mode,
		       st_lis2dw12_selftest_table[2].mode);
}

static ssize_t st_lis2dw12_get_selftest_status(struct device *dev,
					       struct device_attribute *attr,
					       char *buf)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	char *ret;

	switch (hw->st_status) {
	case ST_LIS2DW12_ST_PASS:
		ret = "pass";
		break;
	case ST_LIS2DW12_ST_FAIL:
		ret = "fail";
		break;
	default:
	case ST_LIS2DW12_ST_RESET:
		ret = "na";
		break;
	}

	return sprintf(buf, "%s\n", ret);
}

static ssize_t st_lis2dw12_enable_selftest(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	s16 acc_st_x = 0, acc_st_y = 0, acc_st_z = 0;
	s16 acc_x = 0, acc_y = 0, acc_z = 0;
	u8 data[ST_LIS2DW12_DATA_SIZE], val;
	int i, err, gain;

	mutex_lock(&iio_dev->mlock);

	if (iio_buffer_enabled(iio_dev)) {
		err = -EBUSY;
		goto unlock;
	}

	for (i = 0; i < ARRAY_SIZE(st_lis2dw12_selftest_table); i++)
		if (!strncmp(buf, st_lis2dw12_selftest_table[i].mode,
			     size - 2))
			break;

	if (i == ARRAY_SIZE(st_lis2dw12_selftest_table)) {
		err = -EINVAL;
		goto unlock;
	}

	hw->st_status = ST_LIS2DW12_ST_RESET;
	val = st_lis2dw12_selftest_table[i].val;
	gain = hw->gain;

	/* fs = 2g, odr = 50Hz */
	err = st_lis2dw12_set_fs(hw, ST_LIS2DW12_FS_2G_GAIN);
	if (err < 0)
		goto unlock;

	err = st_lis2dw12_set_odr(hw, 50);
	if (err < 0)
		goto unlock;

	msleep(200);

	for (i = 0; i < 5; i++) {
		err = hw->tf->read(hw->dev, ST_LIS2DW12_OUT_X_L_ADDR,
				   sizeof(data), data);
		if (err < 0)
			goto unlock;

		acc_x += ((s16)get_unaligned_le16(&data[0]) >> 2) / 5;
		acc_y += ((s16)get_unaligned_le16(&data[2]) >> 2) / 5;
		acc_z += ((s16)get_unaligned_le16(&data[4]) >> 2) / 5;

		msleep(10);
	}

	/* enable self test */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL3_ADDR,
					  ST_LIS2DW12_ST_MASK, val);
	if (err < 0)
		goto unlock;

	msleep(200);

	for (i = 0; i < 5; i++) {
		err = hw->tf->read(hw->dev, ST_LIS2DW12_OUT_X_L_ADDR,
				   sizeof(data), data);
		if (err < 0)
			goto unlock;

		acc_st_x += ((s16)get_unaligned_le16(&data[0]) >> 2) / 5;
		acc_st_y += ((s16)get_unaligned_le16(&data[2]) >> 2) / 5;
		acc_st_z += ((s16)get_unaligned_le16(&data[4]) >> 2) / 5;

		msleep(10);
	}

	if (abs(acc_st_x - acc_x) >= ST_LIS2DW12_SELFTEST_MIN &&
	    abs(acc_st_x - acc_x) <= ST_LIS2DW12_SELFTEST_MAX &&
	    abs(acc_st_y - acc_y) >= ST_LIS2DW12_SELFTEST_MIN &&
	    abs(acc_st_y - acc_y) >= ST_LIS2DW12_SELFTEST_MIN &&
	    abs(acc_st_z - acc_z) >= ST_LIS2DW12_SELFTEST_MIN &&
	    abs(acc_st_z - acc_z) >= ST_LIS2DW12_SELFTEST_MIN)
		hw->st_status = ST_LIS2DW12_ST_PASS;
	else
		hw->st_status = ST_LIS2DW12_ST_FAIL;

	/* disable self test */
	err = st_lis2dw12_write_with_mask(hw, ST_LIS2DW12_CTRL3_ADDR,
					  ST_LIS2DW12_ST_MASK, 0);
	if (err < 0)
		goto unlock;

	err = st_lis2dw12_set_fs(hw, gain);
	if (err < 0)
		goto unlock;

	err = st_lis2dw12_sensor_set_enable(hw, false);

unlock:
	mutex_unlock(&iio_dev->mlock);

	return err < 0 ? err : size;
}

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_lis2dw12_sysfs_sampling_frequency_avl);
static IIO_DEVICE_ATTR(in_accel_scale_available, S_IRUGO,
		       st_lis2dw12_sysfs_scale_avail, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark, S_IWUSR | S_IRUGO,
		       st_lis2dw12_get_hwfifo_watermark,
		       st_lis2dw12_set_hwfifo_watermark, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_min, S_IRUGO,
		       st_lis2dw12_get_min_hwfifo_watermark, NULL, 0);
static IIO_DEVICE_ATTR(hwfifo_watermark_max, S_IRUGO,
		       st_lis2dw12_get_max_hwfifo_watermark, NULL, 0);
static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
			      st_lis2dw12_get_sampling_frequency,
			      st_lis2dw12_set_sampling_frequency);
static IIO_DEVICE_ATTR(hwfifo_enabled, S_IWUSR | S_IRUGO,
		       st_lis2dw12_get_hwfifo_enabled,
		       st_lis2dw12_set_hwfifo_enabled, 0);
static IIO_DEVICE_ATTR(hwfifo_flush, S_IWUSR, NULL,
		       st_lis2dw12_flush_fifo, 0);
static IIO_DEVICE_ATTR(selftest_available, S_IRUGO,
		       st_lis2dw12_get_selftest_avail, NULL, 0);
static IIO_DEVICE_ATTR(selftest, S_IWUSR | S_IRUGO,
		       st_lis2dw12_get_selftest_status,
		       st_lis2dw12_enable_selftest, 0);

static struct attribute *st_lis2dw12_acc_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_in_accel_scale_available.dev_attr.attr,
	&iio_dev_attr_hwfifo_enabled.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_min.dev_attr.attr,
	&iio_dev_attr_hwfifo_watermark_max.dev_attr.attr,
	&iio_dev_attr_hwfifo_flush.dev_attr.attr,
	&iio_dev_attr_selftest_available.dev_attr.attr,
	&iio_dev_attr_selftest.dev_attr.attr,
	NULL,
};

static const struct attribute_group st_lis2dw12_acc_attribute_group = {
	.attrs = st_lis2dw12_acc_attributes,
};

static const struct iio_info st_lis2dw12_acc_info = {
	.driver_module = THIS_MODULE,
	.attrs = &st_lis2dw12_acc_attribute_group,
	.read_raw = st_lis2dw12_read_raw,
	.write_raw = st_lis2dw12_write_raw,
	.write_event_config = st_lis2dw12_write_event_config,
};

int st_lis2dw12_probe(struct iio_dev *iio_dev)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err;

	err = st_lis2dw12_check_whoami(hw);
	if (err < 0)
		return err;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = hw->dev;
	iio_dev->channels = st_lis2dw12_acc_channels;
	iio_dev->num_channels = ARRAY_SIZE(st_lis2dw12_acc_channels);
	iio_dev->name = "lis2dw12_accel";
	iio_dev->info = &st_lis2dw12_acc_info;

	mutex_init(&hw->lock);
	hw->gain = st_lis2dw12_fs_table[0].gain;
	hw->odr = st_lis2dw12_odr_table[1].hz;
	hw->watermark = 1;

	err = st_lis2dw12_init_hw(hw);
	if (err < 0)
		return err;

	if (hw->irq > 0) {
		err = st_lis2dw12_init_ring(hw);
		if (err)
			return err;
	}

	err = devm_iio_device_register(hw->dev, iio_dev);
	if (err && hw->irq > 0)
		st_lis2dw12_deallocate_ring(iio_dev);

	return err;
}
EXPORT_SYMBOL(st_lis2dw12_probe);

int st_lis2dw12_remove(struct iio_dev *iio_dev)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);

	if (hw->irq > 0)
		st_lis2dw12_deallocate_ring(iio_dev);

	return 0;
}
EXPORT_SYMBOL(st_lis2dw12_remove);

