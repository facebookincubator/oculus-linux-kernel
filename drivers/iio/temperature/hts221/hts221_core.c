/*
 * STMicroelectronics hts221 sensor driver
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
#include <linux/delay.h>
#include <asm/unaligned.h>

#include "hts221.h"

#define REG_WHOAMI_ADDR		0x0f
#define REG_WHOAMI_VAL		0xbc

#define REG_CNTRL1_ADDR		0x20
#define REG_CNTRL2_ADDR		0x21
#define REG_CNTRL3_ADDR		0x22

#define REG_H_AVG_ADDR		0x10
#define REG_T_AVG_ADDR		0x10
#define REG_H_OUT_L		0x28
#define REG_T_OUT_L		0x2a

#define H_AVG_MASK		0x07
#define T_AVG_MASK		0x38

#define ODR_MASK		0x87
#define BDU_MASK		0x04

#define DRDY_MASK		0x04

#define ENABLE_SENSOR		0x80

#define H_AVG_4			0x00 /* 0.4 %RH */
#define H_AVG_8			0x01 /* 0.3 %RH */
#define H_AVG_16		0x02 /* 0.2 %RH */
#define H_AVG_32		0x03 /* 0.15 %RH */
#define H_AVG_64		0x04 /* 0.1 %RH */
#define H_AVG_128		0x05 /* 0.07 %RH */
#define H_AVG_256		0x06 /* 0.05 %RH */
#define H_AVG_512		0x07 /* 0.03 %RH */

#define T_AVG_2			0x00 /* 0.08 degC */
#define T_AVG_4			0x08 /* 0.05 degC */
#define T_AVG_8			0x10 /* 0.04 degC */
#define T_AVG_16		0x18 /* 0.03 degC */
#define T_AVG_32		0x20 /* 0.02 degC */
#define T_AVG_64		0x28 /* 0.015 degC */
#define T_AVG_128		0x30 /* 0.01 degC */
#define T_AVG_256		0x38 /* 0.007 degC */

/* caldata registers */
#define REG_0RH_CAL_X_H		0x36
#define REG_1RH_CAL_X_H		0x3a
#define REG_0RH_CAL_Y_H		0x30
#define REG_1RH_CAL_Y_H		0x31
#define REG_0T_CAL_X_L		0x3c
#define REG_1T_CAL_X_L		0x3e
#define REG_0T_CAL_Y_H		0x32
#define REG_1T_CAL_Y_H		0x33
#define REG_T1_T0_CAL_Y_H	0x35

struct hts221_odr {
	u32 hz;
	u8 val;
};

struct hts221_avg {
	u8 addr;
	u8 mask;
	struct hts221_avg_avl avg_avl[HTS221_AVG_DEPTH];
};

static const struct hts221_odr hts221_odr_table[] = {
	{ 1, 0x01 },	/* 1Hz */
	{ 7, 0x02 },	/* 7Hz */
	{ 13, 0x03 },	/* 12.5 HZ */
};

static const struct hts221_avg hts221_avg_list[] = {
	{
		.addr = REG_H_AVG_ADDR,
		.mask = H_AVG_MASK,
		.avg_avl = {
			{ 4, H_AVG_4 },
			{ 8, H_AVG_8 },
			{ 16, H_AVG_16 },
			{ 32, H_AVG_32 },
			{ 64, H_AVG_64 },
			{ 128, H_AVG_128 },
			{ 256, H_AVG_256 },
			{ 512, H_AVG_512 },
		},
	},
	{
		.addr = REG_T_AVG_ADDR,
		.mask = T_AVG_MASK,
		.avg_avl = {
			{ 2, T_AVG_2 },
			{ 4, T_AVG_4 },
			{ 8, T_AVG_8 },
			{ 16, T_AVG_16 },
			{ 32, T_AVG_32 },
			{ 64, T_AVG_64 },
			{ 128, T_AVG_128 },
			{ 256, T_AVG_256 },
		},
	},
};

static const struct iio_chan_spec hts221_h_channels[] = {
	{
		.type = IIO_HUMIDITYRELATIVE,
		.address = REG_H_OUT_L,
		.modified = 0,
		.channel2 = IIO_NO_MOD,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_OFFSET) |
				      BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static const struct iio_chan_spec hts221_t_channels[] = {
	{
		.type = IIO_TEMP,
		.address = REG_T_OUT_L,
		.modified = 0,
		.channel2 = IIO_NO_MOD,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |
				      BIT(IIO_CHAN_INFO_OFFSET) |
				      BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = 0,
		.scan_type = {
			.sign = 's',
			.realbits = 16,
			.storagebits = 16,
			.endianness = IIO_LE,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static int hts221_write_with_mask(struct hts221_dev *dev, u8 addr, u8 mask,
				  u8 val)
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

	data = (data & ~mask) | (val & mask);

	err = dev->tf->write(dev->dev, addr, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to write %02x register\n", addr);
		mutex_unlock(&dev->lock);
		return err;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

static int hts221_check_whoami(struct hts221_dev *dev)
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

int hts221_config_drdy(struct hts221_dev *dev, bool enable)
{
	u8 val = (enable) ? 0x04 : 0;

	return hts221_write_with_mask(dev, REG_CNTRL3_ADDR, DRDY_MASK, val);
}

static int hts221_update_odr(struct hts221_dev *dev, u8 odr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(hts221_odr_table); i++)
		if (hts221_odr_table[i].hz == odr)
			break;

	if (i == ARRAY_SIZE(hts221_odr_table))
		return -EINVAL;

	return hts221_write_with_mask(dev, REG_CNTRL1_ADDR, ODR_MASK,
				      ENABLE_SENSOR | BDU_MASK |
				      hts221_odr_table[i].val);
}

static int hts221_sensor_update_odr(struct hts221_sensor *sensor, u8 odr)
{
	int i;
	struct hts221_sensor *cur_sensor;

	for (i = 0; i < HTS221_SENSOR_MAX; i++) {
		cur_sensor = iio_priv(sensor->dev->iio_devs[i]);

		if (cur_sensor == sensor)
			continue;

		if (cur_sensor->enabled && cur_sensor->odr >= odr)
			return 0;
	}

	return hts221_update_odr(sensor->dev, odr);
}

static int hts221_update_avg(struct hts221_sensor *sensor, u16 val)
{
	int i, err;
	const struct hts221_avg *avg = &hts221_avg_list[sensor->type];

	for (i = 0; i < HTS221_AVG_DEPTH; i++)
		if (avg->avg_avl[i].avg == val)
			break;

	if (i == HTS221_AVG_DEPTH)
		return -EINVAL;

	err = hts221_write_with_mask(sensor->dev, avg->addr, avg->mask,
				     avg->avg_avl[i].val);
	if (err < 0)
		return err;

	sensor->cur_avg_idx = i;

	return 0;
}

static ssize_t
hts221_sysfs_get_h_avg_val(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	int idx, val;
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	idx = sensor->cur_avg_idx;
	val = hts221_avg_list[HTS221_SENSOR_H].avg_avl[idx].avg;

	return sprintf(buf, "%d\n", val);
}

static ssize_t
hts221_sysfs_set_h_avg_val(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int err;
	unsigned int val;
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		return err;

	err = hts221_update_avg(sensor, (u16)val);

	return err < 0 ? err : size;
}

static ssize_t
hts221_sysfs_get_t_avg_val(struct device *device,
			   struct device_attribute *attr, char *buf)
{
	int idx, val;
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	idx = sensor->cur_avg_idx;
	val = hts221_avg_list[HTS221_SENSOR_T].avg_avl[idx].avg;

	return sprintf(buf, "%d\n", val);
}

static ssize_t
hts221_sysfs_set_t_avg_val(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t size)
{
	int err;
	unsigned int val;
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	err = kstrtoint(buf, 10, &val);
	if (err < 0)
		return err;

	err = hts221_update_avg(sensor, (u16)val);

	return err < 0 ? err : size;
}

static ssize_t hts221_sysfs_sampling_freq(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int i;
	ssize_t len = 0;

	for (i = 0; i < ARRAY_SIZE(hts221_odr_table); i++)
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				 hts221_odr_table[i].hz);
	buf[len - 1] = '\n';

	return len;
}

static ssize_t
hts221_sysfs_get_sampling_frequency(struct device *device,
				    struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	return sprintf(buf, "%d\n", sensor->odr);
}

static ssize_t
hts221_sysfs_set_sampling_frequency(struct device *device,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int err;
	unsigned int odr;
	struct iio_dev *indio_dev = dev_get_drvdata(device);
	struct hts221_sensor *sensor = iio_priv(indio_dev);

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	err = hts221_sensor_update_odr(sensor, odr);
	if (!err)
		sensor->odr = odr;

	return err < 0 ? err : size;
}

int hts221_sensor_power_on(struct hts221_sensor *sensor)
{
	int err, idx, val;

	idx = sensor->cur_avg_idx;
	val = hts221_avg_list[sensor->type].avg_avl[idx].avg;
	err = hts221_update_avg(sensor, val);
	if (err < 0)
		return err;

	err = hts221_sensor_update_odr(sensor, sensor->odr);
	if (err < 0)
		return err;

	sensor->enabled = true;

	return 0;
}

static int hts221_dev_power_off(struct hts221_dev *dev)
{
	int err;
	u8 data = 0;

	mutex_lock(&dev->lock);

	err = dev->tf->write(dev->dev, REG_CNTRL1_ADDR, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to write %02x register\n",
			REG_CNTRL1_ADDR);
		mutex_unlock(&dev->lock);
		return err;
	}

	err = dev->tf->write(dev->dev, REG_CNTRL2_ADDR, 1, &data);
	if (err < 0) {
		dev_err(dev->dev, "failed to write %02x register\n",
			REG_CNTRL2_ADDR);
		mutex_unlock(&dev->lock);
		return err;
	}

	mutex_unlock(&dev->lock);

	return 0;
}

int hts221_sensor_power_off(struct hts221_sensor *sensor)
{
	int i;
	struct hts221_sensor *cur_sensor;
	struct hts221_dev *dev = sensor->dev;

	for (i = 0; i < HTS221_SENSOR_MAX; i++) {
		cur_sensor = iio_priv(dev->iio_devs[i]);
		if (cur_sensor == sensor)
			continue;

		if (cur_sensor->enabled)
			break;
	}

	if (i == HTS221_SENSOR_MAX)
		hts221_dev_power_off(dev);

	sensor->enabled = false;

	return 0;
}

static int hts221_parse_caldata(struct hts221_sensor *sensor)
{
	int err, *slope, *b_gen;
	u8 addr_x0, addr_x1;
	s16 cal_x0, cal_x1, cal_y0, cal_y1;
	struct hts221_dev *dev = sensor->dev;

	switch (sensor->type) {
	case HTS221_SENSOR_H:
		addr_x0 = REG_0RH_CAL_X_H;
		addr_x1 = REG_1RH_CAL_X_H;

		cal_y1 = 0;
		cal_y0 = 0;
		err = dev->tf->read(dev->dev, REG_0RH_CAL_Y_H, 1,
				    (u8 *)&cal_y0);
		if (err < 0)
			return err;

		err = dev->tf->read(dev->dev, REG_1RH_CAL_Y_H, 1,
				    (u8 *)&cal_y1);
		if (err < 0)
			return err;
		break;
	case HTS221_SENSOR_T: {
		u8 cal0, cal1;

		addr_x0 = REG_0T_CAL_X_L;
		addr_x1 = REG_1T_CAL_X_L;

		err = dev->tf->read(dev->dev, REG_0T_CAL_Y_H, 1, &cal0);
		if (err < 0)
			return err;

		err = dev->tf->read(dev->dev, REG_T1_T0_CAL_Y_H, 1, &cal1);
		if (err < 0)
			return err;
		cal_y0 = (le16_to_cpu(cal1 & 0x3) << 8) | cal0;

		err = dev->tf->read(dev->dev, REG_1T_CAL_Y_H, 1, &cal0);
		if (err < 0)
			return err;

		err = dev->tf->read(dev->dev, REG_T1_T0_CAL_Y_H, 1, &cal1);
		if (err < 0)
			return err;
		cal_y1 = (le16_to_cpu((cal1 & 0xc) >> 2) << 8) | cal0;
		break;
	}
	default:
		return -ENODEV;
	}

	err = dev->tf->read(dev->dev, addr_x0, 2, (u8 *)&cal_x0);
	if (err < 0)
		return err;
	cal_x0 = le32_to_cpu(cal_x0);

	err = dev->tf->read(dev->dev, addr_x1, 2, (u8 *)&cal_x1);
	if (err < 0)
		return err;
	cal_x1 = le32_to_cpu(cal_x1);

	slope = &sensor->slope;
	b_gen = &sensor->b_gen;

	*slope = ((cal_y1 - cal_y0) * 8000) / (cal_x1 - cal_x0);
	*b_gen = (((s32)cal_x1 * cal_y0 - (s32)cal_x0 * cal_y1) * 1000) /
		 (cal_x1 - cal_x0);
	*b_gen *= 8;

	return 0;
}

static int hts221_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *ch,
			   int *val, int *val2, long mask)
{
	int ret;
	struct hts221_sensor *sensor = iio_priv(indio_dev);
	struct hts221_dev *dev = sensor->dev;

	switch (mask) {
	case IIO_CHAN_INFO_RAW: {
		u8 data[2];

		if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED)
			return -EBUSY;

		ret = hts221_sensor_power_on(sensor);
		if (ret < 0)
			return ret;

		msleep(50);

		mutex_lock(&dev->lock);
		ret = dev->tf->read(dev->dev, ch->address, 2, data);
		if (ret < 0) {
			mutex_unlock(&dev->lock);
			return ret;
		}
		mutex_unlock(&dev->lock);

		ret = hts221_sensor_power_off(sensor);
		if (ret < 0) {
			mutex_unlock(&dev->lock);
			return ret;
		}

		*val = (s16)get_unaligned_le16(data);
		ret = IIO_VAL_INT;
		break;
	}
	case IIO_CHAN_INFO_SCALE: {
		s64 tmp;
		s32 rem, div, data = sensor->slope;

		switch (ch->type) {
		case IIO_HUMIDITYRELATIVE: {
			div = (1 << 4) * 1000;
			break;
		}
		case IIO_TEMP: {
			div = (1 << 6) * 1000;
			break;
		}
		default:
			return -EINVAL;
		}

		tmp = div_s64(data * 1000000000LL, div);
		tmp = div_s64_rem(tmp, 1000000000LL, &rem);

		*val = tmp;
		*val2 = rem;
		ret = IIO_VAL_INT_PLUS_NANO;
		break;
	}
	case IIO_CHAN_INFO_OFFSET: {
		s64 tmp;
		s32 rem, div = sensor->slope, data = sensor->b_gen;

		tmp = div_s64(data * 1000000000LL, div);
		tmp = div_s64_rem(tmp, 1000000000LL, &rem);

		*val = tmp;
		*val2 = abs(rem);
		ret = IIO_VAL_INT_PLUS_NANO;
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static IIO_CONST_ATTR(humidityrelative_avg_sample_available,
		      "4 8 16 32 64 128 256 512");
static IIO_DEVICE_ATTR(humidityrelative_avg_sample, S_IWUSR | S_IRUGO,
		       hts221_sysfs_get_h_avg_val,
		       hts221_sysfs_set_h_avg_val, 0);
static IIO_CONST_ATTR(temp_avg_sample_available,
		      "2 4 8 16 32 64 128 256");
static IIO_DEVICE_ATTR(temp_avg_sample, S_IWUSR | S_IRUGO,
		       hts221_sysfs_get_t_avg_val,
		       hts221_sysfs_set_t_avg_val, 0);

static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(hts221_sysfs_sampling_freq);
static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
			      hts221_sysfs_get_sampling_frequency,
			      hts221_sysfs_set_sampling_frequency);

static struct attribute *hts221_h_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_humidityrelative_avg_sample_available.dev_attr.attr,
	&iio_dev_attr_humidityrelative_avg_sample.dev_attr.attr,
	NULL,
};

static const struct attribute_group hts221_h_attribute_group = {
	.attrs = hts221_h_attributes,
};

static const struct iio_info hts221_h_info = {
	.driver_module = THIS_MODULE,
	.attrs = &hts221_h_attribute_group,
	.read_raw = &hts221_read_raw,
};

static struct attribute *hts221_t_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_temp_avg_sample_available.dev_attr.attr,
	&iio_dev_attr_temp_avg_sample.dev_attr.attr,
	NULL,
};

static const struct attribute_group hts221_t_attribute_group = {
	.attrs = hts221_t_attributes,
};

static const struct iio_info hts221_t_info = {
	.driver_module = THIS_MODULE,
	.attrs = &hts221_t_attribute_group,
	.read_raw = hts221_read_raw,
};

static struct iio_dev *hts221_alloc_iio_sensor(struct hts221_dev *dev,
					       enum hts221_sensor_type type)
{
	struct iio_dev *iio_dev;
	struct hts221_sensor *sensor;

	iio_dev = iio_device_alloc(sizeof(*sensor));
	if (!iio_dev)
		return NULL;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = dev->dev;

	sensor = iio_priv(iio_dev);
	sensor->type = type;
	sensor->dev = dev;
	sensor->odr = hts221_odr_table[0].hz;

	switch (type) {
	case HTS221_SENSOR_H:
		iio_dev->channels = hts221_h_channels;
		iio_dev->num_channels = ARRAY_SIZE(hts221_h_channels);
		iio_dev->name = "hts221_rh";
		iio_dev->info = &hts221_h_info;
		sensor->drdy_data_mask = 0x02;
		break;
	case HTS221_SENSOR_T:
		iio_dev->channels = hts221_t_channels;
		iio_dev->num_channels = ARRAY_SIZE(hts221_t_channels);
		iio_dev->name = "hts221_temp";
		iio_dev->info = &hts221_t_info;
		sensor->drdy_data_mask = 0x01;
		break;
	default:
		iio_device_free(iio_dev);
		return NULL;
	}

	return iio_dev;
}

int hts221_probe(struct hts221_dev *dev)
{
	int i, err, count = 0;
	struct hts221_sensor *sensor;

	mutex_init(&dev->lock);

	err = hts221_check_whoami(dev);
	if (err < 0)
		return err;

	err = hts221_update_odr(dev, 1);
	if (err < 0)
		return err;

	for (i = 0; i < HTS221_SENSOR_MAX; i++) {
		dev->iio_devs[i] = hts221_alloc_iio_sensor(dev, i);
		if (!dev->iio_devs[i]) {
			err = -ENOMEM;
			goto power_off;
		}
		sensor = iio_priv(dev->iio_devs[i]);

		err = hts221_update_avg(sensor,
					hts221_avg_list[i].avg_avl[3].avg);
		if (err < 0)
			goto power_off;

		err = hts221_parse_caldata(sensor);
		if (err < 0)
			goto power_off;
	}

	err = hts221_dev_power_off(dev);
	if (err < 0)
		goto iio_device_free;

	if (dev->irq > 0) {
		err = hts221_allocate_buffers(dev);
		if (err < 0)
			goto iio_device_free;

		err = hts221_allocate_triggers(dev);
		if (err) {
			hts221_deallocate_buffers(dev);
			goto iio_device_free;
		}
	}

	for (i = 0; i < HTS221_SENSOR_MAX; i++) {
		err = iio_device_register(dev->iio_devs[i]);
		if (err < 0)
			goto iio_register_err;
		count++;
	}

	return 0;

iio_register_err:
	for (i = count - 1; i >= 0; i--)
		iio_device_unregister(dev->iio_devs[i]);
power_off:
	hts221_dev_power_off(dev);
iio_device_free:
	for (i = 0; i < HTS221_SENSOR_MAX; i++)
		if (dev->iio_devs[i])
			iio_device_free(dev->iio_devs[i]);

	return err;
}
EXPORT_SYMBOL(hts221_probe);

int hts221_remove(struct hts221_dev *dev)
{
	int i, err;

	err = hts221_dev_power_off(dev);

	for (i = 0; i < HTS221_SENSOR_MAX; i++)
		iio_device_unregister(dev->iio_devs[i]);

	if (dev->irq > 0) {
		hts221_deallocate_triggers(dev);
		hts221_deallocate_buffers(dev);
	}

	for (i = 0; i < HTS221_SENSOR_MAX; i++)
		iio_device_free(dev->iio_devs[i]);

	return err;
}
EXPORT_SYMBOL(hts221_remove);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics hts221 sensor driver");
MODULE_LICENSE("GPL v2");
