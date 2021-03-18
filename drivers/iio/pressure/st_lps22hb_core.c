/*
 * STMicroelectronics lps22hb driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Matteo Dameno <matteo.dameno@st.com>
 * Armando Visconti <armando.visconti@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/trigger.h>
#include <linux/delay.h>
#include <linux/iio/buffer.h>
#include <linux/iio/events.h>
#include <asm/unaligned.h>
#include <linux/platform_data/st_lps22hb.h>
#include "st_lps22hb_core.h"


#define LPS22HB_FS_LIST_NUM			4
enum {
	LPS22HB_LWC_MODE = 0,
	LPS22HB_NORMAL_MODE,
	LPS22HB_MODE_COUNT,
};

#define LPS22HB_ADD_CHANNEL(device_type, mask, modif, index, ch2, endian, sbits, rbits, addr, s) \
{ \
	.type = device_type, \
	.modified = modif, \
	.info_mask_separate = mask, \
	.scan_index = index, \
	.channel2 = ch2, \
	.address = addr, \
	.scan_type = { \
		.sign = s, \
		.realbits = rbits, \
		.shift = sbits - rbits, \
		.storagebits = sbits, \
		.endianness = endian, \
	}, \
}

struct lps22hb_odr_reg {
	u32 hz;
	u8 value;
};

static const struct lps22hb_odr_table_t {
	u8 addr;
	u8 mask;
	struct lps22hb_odr_reg odr_avl[LPS22HB_MODE_COUNT][LPS22HB_ODR_LIST_NUM];
} lps22hb_odr_table = {
	.addr = LPS22HB_ODR_ADDR,
	.mask = LPS22HB_ODR_MASK,

	/*
	 * ODR values for Low Power Mode
	 */
	.odr_avl[LPS22HB_LWC_MODE][0] = {.hz = 0,.value = LPS22HB_ODR_POWER_OFF_VAL,},
	.odr_avl[LPS22HB_LWC_MODE][1] = {.hz = 1,.value = LPS22HB_ODR_1HZ_VAL,},
	.odr_avl[LPS22HB_LWC_MODE][2] = {.hz = 10,.value = LPS22HB_ODR_10HZ_VAL,},
	.odr_avl[LPS22HB_LWC_MODE][3] = {.hz = 25,.value = LPS22HB_ODR_25HZ_VAL,},
	.odr_avl[LPS22HB_LWC_MODE][4] = {.hz = 50,.value = LPS22HB_ODR_50HZ_VAL,},
	.odr_avl[LPS22HB_LWC_MODE][5] = {.hz = 75,.value = LPS22HB_ODR_75HZ_VAL,},

	/*
	 * ODR values for High Resolution Mode
	 */
	.odr_avl[LPS22HB_NORMAL_MODE][0] = {.hz = 0,.value = LPS22HB_ODR_POWER_OFF_VAL,},
	.odr_avl[LPS22HB_NORMAL_MODE][1] = {.hz = 1,.value = LPS22HB_ODR_1HZ_VAL,},
	.odr_avl[LPS22HB_NORMAL_MODE][2] = {.hz = 10,.value = LPS22HB_ODR_10HZ_VAL,},
	.odr_avl[LPS22HB_NORMAL_MODE][3] = {.hz = 25,.value = LPS22HB_ODR_25HZ_VAL,},
	.odr_avl[LPS22HB_NORMAL_MODE][4] = {.hz = 50,.value = LPS22HB_ODR_50HZ_VAL,},
	.odr_avl[LPS22HB_NORMAL_MODE][5] = {.hz = 75,.value = LPS22HB_ODR_75HZ_VAL,},
};

static const struct lps22hb_sensors_table {
	const char *name;
	const char *description;
	const u32 min_odr_hz;
	const u8 iio_channel_size;
	const struct iio_chan_spec iio_channel[LPS22HB_MAX_CHANNEL_SPEC];
} lps22hb_sensors_table[LPS22HB_SENSORS_NUMB] = {
	[LPS22HB_PRESS] = {
		.name = "press",
		.description = "ST LPS22HB Pressure Sensor",
		.min_odr_hz = LPS22HB_PRESS_ODR,
		.iio_channel = {
			LPS22HB_ADD_CHANNEL(IIO_PRESSURE,
					BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_SCALE),
					0, 0, IIO_NO_MOD, IIO_LE,
					24, 24, LPS22HB_PRESS_OUT_XL_ADDR, 'u'),
			IIO_CHAN_SOFT_TIMESTAMP(1)
		},
		.iio_channel_size = LPS22HB_PRESS_CHANNEL_SIZE,
	},
	[LPS22HB_TEMP] = {
		.name = "temp",
		.description = "ST LPS22HB Temperature Sensor",
		.min_odr_hz = LPS22HB_TEMP_ODR,
		.iio_channel = {
			LPS22HB_ADD_CHANNEL(IIO_TEMP,
					BIT(IIO_CHAN_INFO_RAW) |
					BIT(IIO_CHAN_INFO_SCALE),
					0, 0, IIO_NO_MOD, IIO_LE,
					16, 16, LPS22HB_TEMP_OUT_L_ADDR, 's'),
			IIO_CHAN_SOFT_TIMESTAMP(1)
		},
		.iio_channel_size = LPS22HB_TEMP_CHANNEL_SIZE,
	},
};

inline int lps22hb_read_register(struct lps22hb_data *cdata, u8 reg_addr, int data_len,
							u8 *data)
{
	return cdata->tf->read(cdata, reg_addr, data_len, data);
}

static int lps22hb_write_register(struct lps22hb_data *cdata, u8 reg_addr,
							u8 mask, u8 data)
{
	int err;
	u8 new_data = 0x00, old_data = 0x00;

	err = lps22hb_read_register(cdata, reg_addr, 1, &old_data);
	if (err < 0)
		return err;

	new_data = ((old_data & (~mask)) | ((data << __ffs(mask)) & mask));
	if (new_data == old_data)
		return 1;

	return cdata->tf->write(cdata, reg_addr, 1, &new_data);
}

int lps22hb_set_fifo_mode(struct lps22hb_data *cdata, enum fifo_mode fm)
{
	u8 reg_value;
	bool enable_fifo;

	switch (fm) {
	case BYPASS:
		reg_value = LPS22HB_FIFO_MODE_BYPASS;
		enable_fifo = false;
		break;
	case STREAM:
		reg_value = LPS22HB_FIFO_MODE_STREAM;
		enable_fifo = true;
		break;
	default:
		return -EINVAL;
	}

	cdata->timestamp = cdata->sensor_timestamp = lps22hb_get_time_ns();

	return lps22hb_write_register(cdata, LPS22HB_FIFO_MODE_ADDR,
				LPS22HB_FIFO_MODE_MASK, reg_value);
}
EXPORT_SYMBOL(lps22hb_set_fifo_mode);

int lps22hb_write_max_odr(struct lps22hb_sensor_data *sdata) {
	int err, i;
	u32 max_odr = 0;
	u8 power_mode = sdata->cdata->power_mode;
	struct lps22hb_sensor_data *t_sdata;

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++)
		if (CHECK_BIT(sdata->cdata->enabled_sensor, i)) {
			t_sdata = iio_priv(sdata->cdata->iio_sensors_dev[i]);
			if (t_sdata->odr > max_odr)
				max_odr = t_sdata->odr;
		}

	if (max_odr != sdata->cdata->common_odr) {
		for (i = 0; i < LPS22HB_ODR_LIST_NUM; i++) {
			if (lps22hb_odr_table.odr_avl[power_mode][i].hz >= max_odr)
				break;
		}
		if (i == LPS22HB_ODR_LIST_NUM)
			return -EINVAL;

		err = lps22hb_write_register(sdata->cdata,
				lps22hb_odr_table.addr,
				lps22hb_odr_table.mask,
				lps22hb_odr_table.odr_avl[power_mode][i].value);
		if (err < 0)
			return err;

		sdata->cdata->common_odr = max_odr;
	}

	return 0;
}

int lps22hb_update_drdy_irq(struct lps22hb_sensor_data *sdata, bool state)
{
	u8 reg_addr, reg_val, reg_mask;

	switch (sdata->sindex) {
	case LPS22HB_PRESS:
	case LPS22HB_TEMP:
		reg_addr = LPS22HB_CTRL3_ADDR;
		reg_mask = (LPS22HB_INT_FTH_MASK);
		if (state)
			reg_val = LPS22HB_EN_BIT;
		else
			reg_val = LPS22HB_DIS_BIT;

		break;

	default:
		return -EINVAL;
	}

	return lps22hb_write_register(sdata->cdata, reg_addr, reg_mask,
				reg_val);
}
EXPORT_SYMBOL(lps22hb_update_drdy_irq);

static int lps22hb_alloc_fifo(struct lps22hb_data *cdata)
{
	int fifo_size;

	fifo_size = LPS22HB_MAX_FIFO_LENGHT * LPS22HB_FIFO_BYTE_FOR_SAMPLE;

	cdata->fifo_data = kmalloc(fifo_size, GFP_KERNEL);
	if (!cdata->fifo_data)
		return -ENOMEM;

	cdata->fifo_size = fifo_size;

	return 0;
}

int lps22hb_update_fifo_ths(struct lps22hb_data *cdata)
{
	int err;
	u8 fifo_len;
	struct iio_dev *indio_dev;

	indio_dev = cdata->iio_sensors_dev[LPS22HB_PRESS];
	fifo_len = (indio_dev->buffer->length);

	err = lps22hb_write_register(cdata, LPS22HB_FIFO_THS_ADDR,
				LPS22HB_FIFO_THS_MASK,
				fifo_len);
	if (err < 0)
		return err;

	return 0;
}
EXPORT_SYMBOL(lps22hb_update_fifo_ths);

int lps22hb_set_enable(struct lps22hb_sensor_data *sdata, bool state)
{
	int err = 0;

	if (sdata->sindex != LPS22HB_PRESS && sdata->sindex != LPS22HB_TEMP)
		return -EINVAL;

	if (sdata->enabled == state)
		return 0;

	/*
	 * Start assuming the sensor enabled if state == true.
	 * It will be restored if an error occur.
	 */
	if (state) {
		SET_BIT(sdata->cdata->enabled_sensor, sdata->sindex);
	} else {
		RESET_BIT(sdata->cdata->enabled_sensor, sdata->sindex);

		/* Touch common device regs only if both are disabled */
		if (sdata->cdata->enabled_sensor != 0)
			goto soft_exit;
	}

	/* Program the device */
	err = lps22hb_update_drdy_irq(sdata, state);
	if (err < 0)
		goto enable_sensor_error;

	err = lps22hb_write_max_odr(sdata);
	if (err < 0)
		goto enable_sensor_error;

soft_exit:
	sdata->enabled = state;

	return 0;

enable_sensor_error:
	if (state) {
		RESET_BIT(sdata->cdata->enabled_sensor, sdata->sindex);
	} else
		SET_BIT(sdata->cdata->enabled_sensor, sdata->sindex);

	return err;
}
EXPORT_SYMBOL(lps22hb_set_enable);

int lps22hb_init_sensors(struct lps22hb_data *cdata)
{
	int err;

	/*
	 * Soft reset the device on power on.
	 */
	err = lps22hb_write_register(cdata, LPS22HB_SOFT_RESET_ADDR,
				LPS22HB_SOFT_RESET_MASK,
				LPS22HB_EN_BIT);
	if (err < 0)
		return err;

	mdelay(40);

	/*
	 * Enable latched interrupt mode.
	 */
	err = lps22hb_write_register(cdata, LPS22HB_LIR_ADDR,
				LPS22HB_LIR_MASK,
				LPS22HB_EN_BIT);
	if (err < 0)
		return err;

	/*
	 * Enable block data update feature.
	 */
	err = lps22hb_write_register(cdata, LPS22HB_BDU_ADDR,
				LPS22HB_BDU_MASK,
				LPS22HB_EN_BIT);
	if (err < 0)
		return err;

	return 0;
}

static ssize_t lps22hb_get_sampling_frequency(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct lps22hb_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	return sprintf(buf, "%d\n", sdata->odr);
}

ssize_t lps22hb_set_sampling_frequency(struct device * dev,
					struct device_attribute * attr,
					const char *buf, size_t count)
{
	int err;
	u8 power_mode, mode_count;
	unsigned int odr, i;
	struct iio_dev *indio_dev = dev_get_drvdata(dev);
	struct lps22hb_sensor_data *sdata = iio_priv(indio_dev);

	err = kstrtoint(buf, 10, &odr);
	if (err < 0)
		return err;

	if (sdata->odr == odr)
		return count;

	power_mode = sdata->cdata->power_mode;
	mode_count = LPS22HB_ODR_LIST_NUM;

	for (i = 0; i < mode_count; i++) {
		if (lps22hb_odr_table.odr_avl[power_mode][i].hz >= odr)
			break;
	}
	if (i == LPS22HB_ODR_LIST_NUM)
		return -EINVAL;

	mutex_lock(&indio_dev->mlock);
	sdata->odr = lps22hb_odr_table.odr_avl[power_mode][i].hz;
	mutex_unlock(&indio_dev->mlock);

	err = lps22hb_write_max_odr(sdata);
	if (err < 0)
		return err;

	return (err < 0) ? err : count;
}

static ssize_t lps22hb_get_sampling_frequency_avail(struct device *dev,
						struct device_attribute
						*attr, char *buf)
{
	int i, len = 0, mode_count, mode;
	struct lps22hb_sensor_data *sdata = iio_priv(dev_get_drvdata(dev));

	mode = sdata->cdata->power_mode;
	mode_count = LPS22HB_ODR_LIST_NUM;

	for (i = 0; i < mode_count; i++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ",
				lps22hb_odr_table.odr_avl[mode][i].hz);
	}
	buf[len - 1] = '\n';

	return len;
}

ssize_t lps22hb_get_hw_fifo_lenght(struct device * dev,
				struct device_attribute * attr, char *buf)
{
	return sprintf(buf, "%d\n", LPS22HB_MAX_FIFO_LENGHT);
}

static int lps22hb_read_raw(struct iio_dev *indio_dev,
			struct iio_chan_spec const *ch, int *val,
							int *val2, long mask)
{
	int err;
	u8 outdata[2], nbytes;
	struct lps22hb_sensor_data *sdata = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		mutex_lock(&indio_dev->mlock);
		if (indio_dev->currentmode == INDIO_BUFFER_TRIGGERED) {
			mutex_unlock(&indio_dev->mlock);
			return -EBUSY;
		}

		err = lps22hb_set_enable(sdata, true);
		if (err < 0) {
			mutex_unlock(&indio_dev->mlock);
			return -EBUSY;
		}

		msleep(40);

		nbytes = ch->scan_type.realbits / 8;

		err = lps22hb_read_register(sdata->cdata, ch->address, nbytes, outdata);
		if (err < 0) {
			mutex_unlock(&indio_dev->mlock);
			return err;
		}

		if (sdata->sindex == LPS22HB_PRESS) {
			*val = (s32)get_unaligned_le32(outdata);
			*val = *val >> ch->scan_type.shift;
		} else if (sdata->sindex == LPS22HB_TEMP) {
			*val = (s16)get_unaligned_le16(outdata);
			*val = *val >> ch->scan_type.shift;
		}

		err = lps22hb_set_enable(sdata, false);
		mutex_unlock(&indio_dev->mlock);

		if (err < 0)
			return err;

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = 0;

		switch (ch->type) {
		case IIO_PRESSURE:
			*val2 = sdata->gainP;
			break;
		case IIO_TEMP:
			*val2 = sdata->gainT;
			break;
		default:
			return -EINVAL;
		}

		return IIO_VAL_INT_PLUS_NANO;

	default:
		return -EINVAL;
	}

	return 0;
}

static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
					lps22hb_get_sampling_frequency,
					lps22hb_set_sampling_frequency);
static IIO_DEV_ATTR_SAMP_FREQ_AVAIL(lps22hb_get_sampling_frequency_avail);
static IIO_DEVICE_ATTR(hw_fifo_lenght, S_IRUGO,
					lps22hb_get_hw_fifo_lenght, NULL, 0);

static struct attribute *lps22hb_press_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_dev_attr_hw_fifo_lenght.dev_attr.attr,
	NULL,
};

static struct attribute *lps22hb_temp_attributes[] = {
	&iio_dev_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	NULL,
};

static const struct attribute_group lps22hb_press_attribute_group = {
	.attrs = lps22hb_press_attributes,
};
static const struct attribute_group lps22hb_temp_attribute_group = {
	.attrs = lps22hb_temp_attributes,
};


static const struct iio_info lps22hb_info[LPS22HB_SENSORS_NUMB] = {
	[LPS22HB_PRESS] = {
		.driver_module = THIS_MODULE,
		.attrs = &lps22hb_press_attribute_group,
		.read_raw = &lps22hb_read_raw,
	},

	[LPS22HB_TEMP] = {
		.driver_module = THIS_MODULE,
		.attrs = &lps22hb_temp_attribute_group,
		.read_raw = &lps22hb_read_raw,
	},
};

#ifdef CONFIG_IIO_TRIGGER
static const struct iio_trigger_ops lps22hb_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = (&lps22hb_trig_set_state),
};
#define LPS22HB_TRIGGER_OPS (&lps22hb_trigger_ops)
#else /*CONFIG_IIO_TRIGGER */
#define LPS22HB_TRIGGER_OPS NULL
#endif /*CONFIG_IIO_TRIGGER */

#ifdef CONFIG_OF
static const struct of_device_id lps22hb_dt_id[] = {
	{.compatible = "st,lps22hb",},
	{},
};

MODULE_DEVICE_TABLE(of, lps22hb_dt_id);

static u32 lps22hb_parse_dt(struct lps22hb_data *cdata)
{
	u32 val;
	struct device_node *np;

	np = cdata->dev->of_node;
	if (!np)
		return -EINVAL;
	/*TODO for this device interrupt pin is only one!!*/
	if (!of_property_read_u32(np, "st,drdy-int-pin", &val) &&
							(val <= 1) && (val > 0))
		cdata->drdy_int_pin = (u8) val;
	else
		cdata->drdy_int_pin = 1;

	return 0;
}
#endif /*CONFIG_OF */

int lps22hb_common_probe(struct lps22hb_data *cdata, int irq)
{
	u8 wai = 0;
	int32_t err, i, n;
	struct iio_dev *piio_dev;
	struct lps22hb_sensor_data *sdata;

	mutex_init(&cdata->tb.buf_lock);

	cdata->fifo_data = 0;

	err = lps22hb_read_register(cdata, LPS22HB_WHO_AM_I_ADDR, 1, &wai);
	if (err < 0) {
		dev_err(cdata->dev, "failed to read Who-Am-I register.\n");

		return err;
	}
	if (wai != LPS22HB_WHO_AM_I_DEF) {
		dev_err(cdata->dev, "Who-Am-I value not valid.\n");

		return -ENODEV;
	}

	if (irq > 0) {
		cdata->irq = irq;
#ifdef CONFIG_OF
		err = lps22hb_parse_dt(cdata);
		if (err < 0)
			return err;
#else /* CONFIG_OF */
		if (cdata->dev->platform_data) {
			cdata->drdy_int_pin = ((struct lps22hb_platform_data *)
					cdata->dev->platform_data)->drdy_int_pin;

			if ((cdata->drdy_int_pin > 1) || (cdata->drdy_int_pin < 1))
				cdata->drdy_int_pin = 1;
		} else
			cdata->drdy_int_pin = 1;
#endif /* CONFIG_OF */

		dev_info(cdata->dev, "driver use DRDY int pin %d\n",
						cdata->drdy_int_pin);
	}

	cdata->common_odr = 0;
	cdata->enabled_sensor = 0;

	/*
	 * Select sensor power mode operation.
	 *
	 * - LPS22HB_LWC_MODE: Low Power. TODO The output data are 10 bits encoded.
	 * - LPS22HB_NORMAL_MODE: High Resolution. TODO 14 bits output data encoding.
	 */
	cdata->power_mode = LPS22HB_MODE_DEFAULT;

	err = lps22hb_alloc_fifo(cdata);
	if (err)
		return err;

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++) {
		piio_dev = iio_device_alloc(sizeof(struct lps22hb_sensor_data *));
		if (piio_dev == NULL) {
			err = -ENOMEM;

			goto iio_device_free;
		}

		cdata->iio_sensors_dev[i] = piio_dev;
		sdata = iio_priv(piio_dev);
		sdata->enabled = false;
		sdata->cdata = cdata;
		sdata->sindex = i;
		sdata->name = lps22hb_sensors_table[i].name;
		sdata->odr = lps22hb_sensors_table[i].min_odr_hz;
		sdata->gainP = LPS22HB_PRESS_FS_AVL_GAIN;
		sdata->gainT = LPS22HB_TEMP_FS_AVL_GAIN;

		piio_dev->channels = lps22hb_sensors_table[i].iio_channel;
		piio_dev->num_channels = lps22hb_sensors_table[i].iio_channel_size;
		piio_dev->info = &lps22hb_info[i];
		piio_dev->modes = INDIO_DIRECT_MODE;
		piio_dev->name = kasprintf(GFP_KERNEL, "%s_%s", cdata->name,
								sdata->name);
	}

	err = lps22hb_init_sensors(cdata);
	if (err < 0)
		goto iio_device_free;

	err = lps22hb_allocate_rings(cdata);
	if (err < 0)
		goto iio_device_free;

	if (irq > 0) {
		err = lps22hb_allocate_triggers(cdata, LPS22HB_TRIGGER_OPS);
		if (err < 0)
			goto deallocate_ring;
	}

	for (n = 0; n < LPS22HB_SENSORS_NUMB; n++) {
		err = iio_device_register(cdata->iio_sensors_dev[n]);
		if (err)
			goto iio_device_unregister_and_trigger_deallocate;
	}

	dev_info(cdata->dev, "%s: probed\n", LPS22HB_DEV_NAME);
	return 0;

iio_device_unregister_and_trigger_deallocate:
	for (n--; n >= 0; n--)
		iio_device_unregister(cdata->iio_sensors_dev[n]);

deallocate_ring:
	lps22hb_deallocate_rings(cdata);

iio_device_free:
	for (i--; i >= 0; i--)
		iio_device_free(cdata->iio_sensors_dev[i]);

	return err;
}
EXPORT_SYMBOL(lps22hb_common_probe);

void lps22hb_common_remove(struct lps22hb_data *cdata, int irq)
{
	int i;

	if (cdata->fifo_data) {
		kfree(cdata->fifo_data);
		cdata->fifo_size = 0;
	}

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++)
		iio_device_unregister(cdata->iio_sensors_dev[i]);

	if (irq > 0)
		lps22hb_deallocate_triggers(cdata);

	lps22hb_deallocate_rings(cdata);

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++)
		iio_device_free(cdata->iio_sensors_dev[i]);
}

EXPORT_SYMBOL(lps22hb_common_remove);

#ifdef CONFIG_PM
int lps22hb_common_suspend(struct lps22hb_data *cdata)
{
	return 0;
}

EXPORT_SYMBOL(lps22hb_common_suspend);

int lps22hb_common_resume(struct lps22hb_data *cdata)
{
	return 0;
}

EXPORT_SYMBOL(lps22hb_common_resume);
#endif /* CONFIG_PM */

MODULE_DESCRIPTION("STMicroelectronics lps22hb driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");
