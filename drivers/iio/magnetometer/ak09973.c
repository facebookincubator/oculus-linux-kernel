/* drivers/staging/iio/magnetometer/ak09973.c
 *
 * ak09973.c -- A sensor driver for the 3D magnetometer AK09973.
 *
 * Copyright (C) 2020 Asahi Kasei Microdevices Corporation
 *                       Date        Revision
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                      20/09/18	    1.0
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/acpi.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

//#define KERNEL_3_18_XX

//#define AK09973_DEBUG

#ifdef AK09973_DEBUG
#define akdbgprt printk
#else
#define akdbgprt(format, arg...) do {} while (0)
#endif


/*
 * Register definitions, as well as various shifts and masks to get at the
 * individual fields of the registers.
 */

// REGISTER MAP
#define AK09973_REG_WIA			0x00
#define AK09973_DEVICE_ID		0x48C1

#define AK09973_REG_WORD_ST             0x10
#define AK09973_REG_WORD_ST_X           0x11
#define AK09973_REG_WORD_ST_Y           0x12
#define AK09973_REG_WORD_ST_Y_X         0x13
#define AK09973_REG_WORD_ST_Z           0x14
#define AK09973_REG_WORD_ST_Z_X         0x15
#define AK09973_REG_WORD_ST_Z_Y         0x16
#define AK09973_REG_WORD_ST_Z_Y_X       0x17
#define AK09973_REG_BYTE_ST_V           0x18
#define AK09973_REG_BYTE_ST_X           0x19
#define AK09973_REG_BYTE_ST_Y           0x1A
#define AK09973_REG_BYTE_ST_Y_X         0x1B
#define AK09973_REG_BYTE_ST_Z           0x1C
#define AK09973_REG_BYTE_ST_Z_X         0x1D
#define AK09973_REG_BYTE_ST_Z_Y         0x1E
#define AK09973_REG_BYTE_ST_Z_Y_X       0x1F
#define AK09973_REG_CNTL1               0x20
#define AK09973_REG_CNTL2               0x21
#define AK09973_REG_THX                 0x22
#define AK09973_REG_THY                 0x23
#define AK09973_REG_THZ                 0x24
#define AK09973_REG_THV                 0x25
#define AK09973_REG_SRST                0x30

#define AK09973_MAX_REGS      AK09973_REG_SRST

#define AK09973_MEASUREMENT_WAIT_TIME   2

static int modeBitTable[] = {
	0,  0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE, 0x10
};
// 0 : Power Down
// 1 : Measurement Mode 1
// 2 : Measurement Mode 2
// 3 : Measurement Mode 3
// 4 : Measurement Mode 4
// 5 : Measurement Mode 5
// 6 : Measurement Mode 6
// 7 : Measurement Mode 7


static int measurementFreqTable[] = {
	0,     5,    10,    20,    50,   100,    500,    1000,   2000
//	0.0,  5Hz,  10Hz,  20Hz,  50Hz,  100Hz,  500Hz,  1000Hz, 2000
};

enum {
	AK09973_MSRNO_WORD_ST = 0,
	AK09973_MSRNO_WORD_ST_X,     // 1
	AK09973_MSRNO_WORD_ST_Y,     // 2
	AK09973_MSRNO_WORD_ST_Y_X,   // 3
	AK09973_MSRNO_WORD_ST_Z,     // 4
	AK09973_MSRNO_WORD_ST_Z_X,   // 5
	AK09973_MSRNO_WORD_ST_Z_Y,   // 6
	AK09973_MSRNO_WORD_ST_Z_Y_X, // 7
	AK09973_MSRNO_WORD_ST_V,     // 8
	AK09973_MSRNO_BYTE_ST_X,     // 9
	AK09973_MSRNO_BYTE_ST_Y,     // 10
	AK09973_MSRNO_BYTE_ST_Y_X,   // 11
	AK09973_MSRNO_BYTE_ST_Z,     // 12
	AK09973_MSRNO_BYTE_ST_Z_X,   // 13
	AK09973_MSRNO_BYTE_ST_Z_Y,   // 14
	AK09973_MSRNO_BYTE_ST_Z_Y_X, // 15
};

static int msrDataBytesTable[] = {
	1,  // AK09973_REG_WORD_ST
	3,  // AK09973_REG_WORD_ST_X
	3,  // AK09973_REG_WORD_ST_Y
	5,  // AK09973_REG_WORD_ST_Y_X
	3,  // AK09973_REG_WORD_ST_Z
	5,  // AK09973_REG_WORD_ST_Z_X
	5,  // AK09973_REG_WORD_ST_Z_Y
	7,  // AK09973_REG_WORD_ST_Z_Y_X
	5,  // AK09973_REG_WORD_ST_V
	2,  // AK09973_REG_BYTE_ST_X
	2,  // AK09973_REG_BYTE_ST_Y
	3,  // AK09973_REG_BYTE_ST_Y_X
	2,  // AK09973_REG_BYTE_ST_Z
	3,  // AK09973_REG_BYTE_ST_Z_X
	3,  // AK09973_REG_BYTE_ST_Z_Y
	4,  // AK09973_REG_BYTE_ST_Z_Y_X
};

/*
 * Per-instance context data for the device.
 */
struct ak09973_data {
	struct i2c_client *client;
	struct iio_trigger *trig;
	struct regulator *vdd;
	struct mutex lock;
	int int_gpio;
	int irq;

	u8  mode;
	s16 numMode;

	u8 msrNo;

	u8 DRDYENbit;
	u8 SWXENbit;
	u8 SWYENbit;
	u8 SWZENbit;
	u8 SWVENbit;
	u8 ERRENbit;

	u8 POLXbit;
	u8 POLYbit;
	u8 POLZbit;
	u8 POLVbit;

	u8 SDRbit;
	u8 SMRbit;

	u16 BOPXbits;
	u16 BRPXbits;

	u16 BOPYbits;
	u16 BRPYbits;

	u16 BOPZbits;
	u16 BRPZbits;

	u16 BOPVbits;
	u16 BRPVbits;

};

// ****************** Register R/W  ************************************
static int ak09973_i2c_read(struct i2c_client *client, u8 address)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, address);

	if (ret < 0)
		dev_err(&client->dev, "[AK09973] I2C Read Error\n");

	return ret;
}

static int ak09973_i2c_reads(
struct i2c_client *client,
u8 *reg,
int reglen,
u8 *rdata,
int datalen)
{
	struct i2c_msg xfer[2];
	int ret;

	/* Write register */
	xfer[0].addr = client->addr;
	xfer[0].flags = 0;
	xfer[0].len = reglen;
	xfer[0].buf = reg;

	/* Read data */
	xfer[1].addr = client->addr;
	xfer[1].flags = I2C_M_RD;
	xfer[1].len = datalen;
	xfer[1].buf = rdata;

	ret = i2c_transfer(client->adapter, xfer, 2);

	if (ret == 2)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int ak09973_i2c_read8(struct i2c_client *client,
		u8 address, int rLen, u8 *rdata)
{
	u8  tx[1];
	int i, ret;

	akdbgprt("[AK09973] %s address=%XH\n", __func__,  (int)address);

	if (rLen > 8) {
		dev_err(&client->dev, "[AK09973] %s Read Word Length Error %d\n", __func__, rLen);
		return -EINVAL;
	}

	tx[0] = address;

	ret = ak09973_i2c_reads(client, tx, 1, rdata, rLen);
	if (ret < 0) {
		dev_err(&client->dev, "[AK09973] I2C Read Error\n");
		for (i = 0; i < rLen; i++)
			rdata[i] = 0;
	}

	return(ret);

}

static int ak09973_i2c_read16(struct i2c_client *client,
		u8 address, int wordLen, u16 *rdata)
{
	u8  tx[1];
	u8  rx[8];
	int i, ret;

	if (wordLen > 4) {
		dev_err(&client->dev, "[AK09973] %s Read Word Length Error %d\n",
				__func__, wordLen);
		return -EINVAL;
	}

	tx[0] = address;

	ret = ak09973_i2c_reads(client, tx, 1, rx, (2 * wordLen));
	if (ret < 0) {
		dev_err(&client->dev, "[AK09973] I2C Read Error\n");
		for (i = 0; i < wordLen; i++)
			rdata[i] = 0;
	} else {
		akdbgprt("[AK09973] %s rx[0]=%02X rx[1]=%02X\n",
				__func__, (int)rx[0], (int)rx[1]);
		for (i = 0; i < wordLen; i++)
			rdata[i] = ((u16)rx[2*i] << 8) + (u16)rx[2*i + 1];
	}

	return ret;
}

// for read measurement data
// 1st  read data : Status 1byte
// 2nd- read data : Status 2byte
static int ak09973_i2c_read8_16(struct i2c_client *client,
		u8 address, int wordLen, u16 *rdata)
{
	u8  tx[1];
	u8  rx[8];
	int i, ret;

	if ((wordLen < 1) || (wordLen > 4)) {
		dev_err(&client->dev, "[AK09973] %s Read Word Length Error %d\n", __func__, wordLen);
		return -EINVAL;
	}

	tx[0] = address;

	ret = ak09973_i2c_reads(client, tx, 1, rx, ((2 * wordLen) - 1));
	if (ret < 0) {
		dev_err(&client->dev, "[AK09973] I2C Read Error\n");
		for (i = 0; i < wordLen; i++)
			rdata[i] = 0;
	} else {
		rdata[0] = (u16)rx[0];
		for (i = 1; i < wordLen; i++)
			rdata[i] = ((u16)rx[2*i-1] << 8) + (u16)rx[2*i];
	}

#ifdef AK09973_DEBUG
	akdbgprt("[AK09973] %s,addr=%XH", __func__, address);
	for (i = 0; i < ((2 * wordLen) - 1); i++)
		akdbgprt(",%02X", (int)rx[i]);
	akdbgprt("\n");
#endif

	return ret;
}

// for read measurement data
// all read data byte
// this function output1 word data
static int ak09973_i2c_read8_word(struct i2c_client *client,
		u8 address, int wordLen, u16 *rdata)
{
	u8  tx[1];
	u8  rx[8];
	int i, ret;

	if ((wordLen < 1) || (wordLen > 4)) {
		dev_err(&client->dev, "[AK09973] %s Read Word Length Error %d\n", __func__, wordLen);
		return -EINVAL;
	}

	tx[0] = address;

	ret = ak09973_i2c_reads(client, tx, 1, rx, wordLen);
	if (ret < 0) {
		dev_err(&client->dev, "[AK09973] I2C Read Error\n");
		for (i = 0; i < wordLen; i++)
			rdata[i] = 0;
	} else {
		rdata[0] = (u16)rx[0];
		for (i = 1; i < wordLen; i++)
			rdata[i] = ((u16)rx[i] << 8);
	}

#ifdef AK09973_DEBUG
	akdbgprt("[AK09973] %s,addr=%XH", __func__, address);
	for (i = 0; i < wordLen; i++)
		akdbgprt(",%02X", (int)rx[i]);
	akdbgprt("\n");
#endif

	return(ret);

}

static int ak09973_i2c_writes(struct i2c_client *client,
		const u8 *tx, size_t wlen)
{
	int ret;

	if (wlen > 1)
		akdbgprt("[AK09973] %s tx[0]=%02x tx[1]=%02x wlen=%d\n",
				__func__, (int)tx[0], (int)tx[1], (int)wlen);


	ret = i2c_master_send(client, tx, wlen);

	if (ret != wlen) {
		pr_err("%s: comm error, ret %d, wlen %d\n", __func__, ret, (int)wlen);
		//dbg_show(__func__);
	}

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

static int ak09973_i2c_write8(struct i2c_client *client,
											u8 address, u8 value)
{
	int ret;

	akdbgprt("[AK09973] %s (0x%02X, 0x%02X)\n", __func__,
									  (int)address, (int)value);

	ret = i2c_smbus_write_byte_data(client, address, value);
	if (ret < 0) {
		pr_err("%s: comm error, ret= %d\n", __func__, ret);
		//dbg_show(__func__);
	}

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

static s32 ak09973_i2c_write16(struct i2c_client *client,
				u8 address, int valueNum, u16 value1, u16 value2)
{
	u8  tx[5];
	s32 ret;
	int n;

	if ((valueNum != 1) &&  (valueNum != 2)) {
		pr_err("%s: valueNum error, valueNum= %d\n", __func__, valueNum);
		return -EINVAL;
	}

	n = 0;

	tx[n++] = address;
	tx[n++] = (u8)((0xFF00 & value1) >> 8);
	tx[n++] = (u8)(0xFF & value1);

	akdbgprt("[AK09973] %s %02XH,%02XH,%02XH\n", __func__,
			(int)tx[0], (int)tx[1], (int)tx[2]);
	if (valueNum == 2) {
		tx[n++] = (u8)((0xFF00 & value2) >> 8);
		tx[n++] = (u8)(0xFF & value2);
	}

	ret = ak09973_i2c_writes(client, tx, n);

	pr_debug("%s return %d\n", __func__, ret);

	return ret;
}

/*********************************************************************/

#define AK099XX_STATUS_CHANNEL(index) {	\
	.type = IIO_MAGN,\
	.modified = 1,\
	.channel2 = IIO_NO_MOD,\
	.info_mask_separate = \
		BIT(IIO_CHAN_INFO_RAW),\
	.scan_index = index,\
	.scan_type = {\
		.sign = 'u',\
		.realbits = 16,\
		.storagebits = 16,\
	},\
}

#define AK099XX_MAG_CHANNEL(axis, index) {	\
	.type = IIO_MAGN,\
	.modified = 1,\
	.channel2 = IIO_##axis,\
	.info_mask_separate = \
		BIT(IIO_CHAN_INFO_RAW) |\
		BIT(IIO_CHAN_INFO_SCALE),\
	.info_mask_shared_by_type = \
		BIT(IIO_CHAN_INFO_SAMP_FREQ),\
	.scan_index = index,\
	.scan_type = {\
		.sign = 'u',\
		.realbits = 16,\
		.storagebits = 16,\
		.shift = 0,\
	},\
}

static const struct iio_chan_spec ak09973_channels[] = {
	AK099XX_STATUS_CHANNEL(0),
	AK099XX_MAG_CHANNEL(MOD_Z, 1),
	AK099XX_MAG_CHANNEL(MOD_Y, 2),
	AK099XX_MAG_CHANNEL(MOD_X, 3),
	IIO_CHAN_SOFT_TIMESTAMP(4),
};

/*********************************************************************/
static ssize_t attr_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct ak09973_data *akm = iio_priv(dev_to_iio_dev(dev));
	u8  cntl2[1];
	u16 result[1];
	u16 rdata[2];
	s16 msrData[12];
	int i, j, n;
	int ret;

	dev_dbg(dev, "[AK09973] %s called", __func__);

	ret = ak09973_i2c_read16(akm->client, AK09973_REG_WORD_ST, 1, result);
	if (ret < 0)
		return ret;

	ak09973_i2c_read16(akm->client, AK09973_REG_CNTL1, 1, rdata);
	dev_info(dev, "[AK09973] CNTL1(20H)=%04XH", (int)rdata[0]);

	ak09973_i2c_read8(akm->client, AK09973_REG_CNTL2, 1, cntl2);
	dev_info(dev, "[AK09973] CNTL2(21H)=%02XH", (int)cntl2[0]);

	n = 0;
	for (i = AK09973_REG_THX; i <= AK09973_REG_THV; i++) {
		ak09973_i2c_read16(akm->client, i, 2, rdata);
		for (j = 0; j < 2; j++) {
			if (rdata[j] < 32768)
				msrData[n] = rdata[j];
			else
				msrData[n] = -((s16)(~rdata[j]) + 1);
			n++;
		}
	}

	dev_info(dev, "[AK09973] (22H) BOPX=%d, BRPX=%d",
									(int)msrData[0], (int)msrData[1]);
	dev_info(dev, "[AK09973] (23H) BOPY=%d, BRPY=%d",
									(int)msrData[2], (int)msrData[3]);
	dev_info(dev, "[AK09973] (24H) BOPZ=%d, BRPZ=%d",
									(int)msrData[4], (int)msrData[5]);
	dev_info(dev, "[AK09973] (25H) BOPV=%d, BRPV=%d",
									(int)msrData[6], (int)msrData[7]);

	result[0] &= 0x3FF;
	return snprintf(buf, 5, "%04X\n", result[0]);
}

static ssize_t attr_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ak09973_data *akm = iio_priv(dev_to_iio_dev(dev));
	char *ptr_data = (char *)buf;
	char *p;
	int  pt_count = 0;
	long val[3];
	unsigned char wdata[10];
	int n;
	int ret;

	dev_dbg(dev, "[AK09973] %s called: '%s'(%zu)", __func__, buf, count);

	if (buf == NULL)
		return -EINVAL;

	if (count == 0)
		return 0;

	for (n = 0; n < 3; n++)
		val[n] = 0;

	while ((p = strsep(&ptr_data, ","))) {
		if (!*p)
			break;

		if (pt_count >= 3)
			break;

		if ((pt_count != 0) && (val[0] > 0x21))
			val[pt_count] = simple_strtol(p, NULL, 10);
		else
			val[pt_count] = simple_strtol(p, NULL, 16);

		pt_count++;
	}

	if ((pt_count < 1) ||
			(((val[0] >= 0x20) && (val[0] != 0x21)) && (pt_count < 3))) {
		dev_err(dev, "[AK09973] %s pt_count = %d, Error", __func__, pt_count);
		return -EINVAL;
	}

	n = 0;
	wdata[n++] = val[0];
	ret = 0;

	if ((val[0] >= 0x20) && (pt_count > 1)) {
		switch (val[0]) {
		case 0x20:
			wdata[n++] = (unsigned char)(0xFF & val[1]);
			wdata[n++] = (unsigned char)(0xFF & val[2]);
			break;
		case 0x21:
			wdata[n++] = (unsigned char)(0xFF & val[1]);
			break;
		case 0x22:
		case 0x23:
		case 0x24:
		case 0x25:
			if (val[1] < 0)
				val[1] +=  65536;
			wdata[n++] = (unsigned char)((0xFF00 & val[1]) >> 8);
			wdata[n++] = (unsigned char)(0xFF & val[1]);
			if (val[2] < 0)
				val[2] +=  65536;
			wdata[n++] = (unsigned char)((0xFF00 & val[2]) >> 8);
			wdata[n++] = (unsigned char)(0xFF & val[2]);
			break;
		default:
			dev_err(dev, "[AK09973] %s Address Error", __func__);
			return -EINVAL;
		}
		ret = ak09973_i2c_writes(akm->client, wdata, n);
	} else if ((val[0] >= AK09973_REG_WORD_ST)
							&& (val[0] <= AK09973_REG_BYTE_ST_Z_Y_X)) {
		akm->msrNo = val[0] - AK09973_REG_WORD_ST;
		akdbgprt("[AK09973] %smeasurement No = %d, address=%XH\n",
					__func__, akm->msrNo, (int)val[0]);
	}

	if (ret < 0)
		return ret;

	return count;
}

static IIO_DEVICE_ATTR(reg,
		0660, /* S_IWUSR | S_IWGRP | S_IRUSR | S_IRGRP */
		attr_reg_show,
		attr_reg_store,
		0);


static struct attribute *ak09973_attributes[] = {
	&iio_dev_attr_reg.dev_attr.attr,
	NULL
};
static const struct attribute_group ak09973_attrs_group = {
	.attrs = ak09973_attributes,
};

/*********************************************************************/

static int ak09973_write_mode(struct ak09973_data *akm, int modeBit)
{
	int mode;
	int ret;

	mode = (akm->SMRbit << 6) + (akm->SDRbit << 5) + modeBit;

	ret = ak09973_i2c_write8(akm->client, AK09973_REG_CNTL2, mode);

	return ret;
}

static int ak09973_read_axis(struct ak09973_data *akm, int index, int *val)
{
	int address;
	u16  rdata[2];
	u16  wordLen;

	akdbgprt("[AK09973] %s index = %d\n", __func__, (int)index);

	if ((index < 0) || (index > 3)) {
		dev_err(&akm->client->dev, "[AK09973] %s index Error index = %d\n", __func__, index);
		return -EINVAL;
	}

	if (akm->mode == 0) {
		ak09973_write_mode(akm, 1);
		msleep(AK09973_MEASUREMENT_WAIT_TIME);
	}

	wordLen = 2;
	switch (index) {
	case 1:
		address = AK09973_REG_WORD_ST_Z;
		break;
	case 2:
		address = AK09973_REG_WORD_ST_Y;
		break;
	case 3:
		address = AK09973_REG_WORD_ST_X;
		break;
	case 0:
	default:
		address = AK09973_REG_WORD_ST;
		wordLen = 1;
		break;
	}

	ak09973_i2c_read8_16(akm->client, address, wordLen, rdata);

	*val = rdata[wordLen - 1];
	if (index == 0)
		*val &= 0x3FF;

	if (*val >= 32768)
		*val -= 65536;

	return 0;
}

static int ak09973_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct ak09973_data *akm = iio_priv(indio_dev);
	int readValue;
	int ret;

	akdbgprt("%s called (index=%d)", __func__, chan->scan_index);

	switch (mask) {
	case IIO_CHAN_INFO_RAW:
		ret = ak09973_read_axis(akm, chan->scan_index, &readValue);
		akdbgprt("[AK09973] %s : scan_index=%d, readValue=%X\n", __func__, chan->scan_index, readValue);
		*val = readValue;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		*val = 1;
		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SAMP_FREQ:
		readValue = ak09973_i2c_read(akm->client, AK09973_REG_CNTL2);
		readValue &= 0x1F;
		akdbgprt("[AK09973] %s : mode=%d, MODEbits=%X\n", __func__, akm->mode, readValue);
		*val = measurementFreqTable[akm->mode];
		return IIO_VAL_INT;
	}

	return -EINVAL;
}


static int ak09973_set_mode(struct ak09973_data *akm, int val)
{
	int n;

	akdbgprt("[AK09973] %s freq = %d\n", __func__, (int)val);

	if (val < 0) {
		dev_err(&akm->client->dev, "[AK09973] %s Val Error val = %d\n", __func__, val);
		return -EINVAL;
	}

	n = 0;

	while ((val > measurementFreqTable[n]) && (n < (akm->numMode - 1)))
		n++;

	akm->mode = n;
	ak09973_write_mode(akm, modeBitTable[akm->mode]);

	akdbgprt("[AK09973] %s mode = %d\n", __func__,  akm->mode);

	return 0;
}

static int ak09973_write_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int val, int val2, long mask)
{
	struct ak09973_data *akm = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		return ak09973_set_mode(akm, val);
	}
	return -EINVAL;
}

static const struct iio_info ak09973_info = {
	.attrs = &ak09973_attrs_group,
	.read_raw = &ak09973_read_raw,
	.write_raw = &ak09973_write_raw,
};


// ********************************************************************
static void ak09973_send_event(struct iio_dev *indio_dev)
{
	struct ak09973_data *akm = iio_priv(indio_dev);
	struct i2c_client *client = akm->client;
	int i;
	int wordLen;
	int msrDataAddr;
	u16 rdata[4];
	/* data(16bit) * 3-axis + status(16bit) + timestamp(64bit) */
	u8  event[sizeof(u16) * 4 + sizeof(s64)];
	u16 *pevent;

	akdbgprt("[AK09973] %s(%d)\n", __func__, __LINE__);

	mutex_lock(&akm->lock);

	msrDataAddr = akm->msrNo + AK09973_REG_WORD_ST;
	if (akm->mode < akm->numMode) {
		if (akm->msrNo < AK09973_MSRNO_BYTE_ST_X) {
			wordLen = (msrDataBytesTable[akm->msrNo] + 1) / 2;
			ak09973_i2c_read8_16(client, msrDataAddr, wordLen, rdata);
		} else {
			wordLen = msrDataBytesTable[akm->msrNo];
			ak09973_i2c_read8_word(client, msrDataAddr, wordLen, rdata);
		}
		akdbgprt("[AK09973] %s rx=%X,%d\n", __func__, (int)rdata[0], (int)rdata[1]);
	} else
		return;

	memset(event, 0, sizeof(event));

	pevent = (u16 *)&event[0];
	for (i = 0; i < wordLen; i++)
		pevent[i] = rdata[i];

#ifdef AK09973_DEBUG
	for (i = 0; i < 8; i += 2)
		akdbgprt("[AK09973] %s %d %02XH,%02XH\n", __func__, i, event[i], event[i+1]);
#endif


#ifdef KERNEL_3_18_XX
	iio_push_to_buffers_with_timestamp(indio_dev, event, iio_get_time_ns());
#else
	iio_push_to_buffers_with_timestamp(indio_dev, event, iio_get_time_ns(indio_dev));
#endif

	mutex_unlock(&akm->lock);

	return;

}

static irqreturn_t ak09973_handle_trigger(int irq, void *p)
{
	const struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;

	akdbgprt("[AK09973] %s(%d)\n", __func__, __LINE__);

	ak09973_send_event(indio_dev);
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

// ********************************************************************
static int ak09973_setup(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak09973_data *akm = iio_priv(indio_dev);
	s32 err;
	u16  deviceID[1];
	u8   value;
	u16  value1;

	akdbgprt("[AK09973] %s(%d)\n", __func__, __LINE__);

	if (!IS_ERR(akm->vdd)) {
		err = regulator_enable(akm->vdd);
		if (err) {
			dev_err(&client->dev, "[AK09973] Failed to enable VDD regulator\n");
			return err;
		}
	}

	err = ak09973_i2c_read16(client, AK09973_REG_WIA, 1, deviceID);
	if (err < 0) {
		pr_err("[AK09973]i2c read failure, AK09973_REG_WIA err=%d\n", err);
		err = -EIO;
		goto setup_err;
	}

	akdbgprt("[AK09973] Device ID = %X\n", deviceID[0]);

	if (deviceID[0] != AK09973_DEVICE_ID) {
		pr_err("[AK09973] Device ID Error, Device ID =0x%X\n", deviceID[0]);
#ifndef AK09973_DEBUG
		err = -EINVAL;
		goto setup_err;
#endif
	}

	value1 = (u16)(akm->POLXbit + (akm->POLYbit << 1)
			+ (akm->POLZbit << 2) + (akm->POLVbit << 3));
	value1 <<= 8;
	value1 += ((u16)(akm->DRDYENbit + (akm->SWXENbit << 1)
			+ (akm->SWYENbit << 2) + (akm->SWZENbit << 3)
			+ (akm->SWVENbit << 4) + (akm->ERRENbit << 5)));

	err = ak09973_i2c_write16(client, AK09973_REG_CNTL1, 1, value1, 0);

	if (akm->mode < ARRAY_SIZE(modeBitTable))
		value = modeBitTable[akm->mode];
	else
		value = 0;

	err = ak09973_write_mode(akm, value);
	err = ak09973_i2c_write16(client, AK09973_REG_THX, 2,
								akm->BOPXbits, akm->BRPXbits);
	err = ak09973_i2c_write16(client, AK09973_REG_THY, 2,
								akm->BOPYbits, akm->BRPYbits);
	err = ak09973_i2c_write16(client, AK09973_REG_THZ, 2,
								akm->BOPZbits, akm->BRPZbits);
	err = ak09973_i2c_write16(client, AK09973_REG_THV, 2,
								akm->BOPVbits, akm->BRPVbits);

	return 0;

setup_err:
	if (!IS_ERR(akm->vdd))
		regulator_disable(akm->vdd);

	return err;
}


static int ak09973_parse_dt(struct ak09973_data  *ak09973)
{
	u32 buf[8];
	struct device *dev;
	struct device_node *np;
	int ret;

	akdbgprt("[AK09973] %s(%d)\n", __func__, __LINE__);

	dev = &(ak09973->client->dev);
	np = dev->of_node;

	if (!np)
		return -EINVAL;

	ret = of_property_read_u32_array(np, "ak09973,measurment_number", buf, 1);
	if (ret < 0)
		ak09973->msrNo = AK09973_MSRNO_WORD_ST_Z_Y_X;
	else {
		ak09973->msrNo = buf[0];
		if (buf[0] > AK09973_MSRNO_BYTE_ST_Z_Y_X)
			ak09973->msrNo = AK09973_MSRNO_WORD_ST_Z_Y_X;
	}

	ret = of_property_read_u32_array(np, "ak09973,DRDY_event", buf, 1);
	if (ret < 0)
		ak09973->DRDYENbit = 1;
	else
		ak09973->DRDYENbit = buf[0];

	ret = of_property_read_u32_array(np, "ak09973,ERR_event", buf, 1);
	if (ret < 0)
		ak09973->ERRENbit = 1;
	else
		ak09973->ERRENbit = buf[0];

	ret = of_property_read_u32_array(np, "ak09973,POL_setting", buf, 4);
	if (ret < 0) {
		ak09973->POLXbit = 0;
		ak09973->POLYbit = 0;
		ak09973->POLZbit = 0;
		ak09973->POLVbit = 0;
	} else {
		ak09973->POLXbit = buf[0];
		ak09973->POLYbit = buf[1];
		ak09973->POLZbit = buf[2];
		ak09973->POLVbit = buf[3];
	}

	ak09973->mode = 0;

	ret = of_property_read_u32_array(np, "ak09973,SDR_setting", buf, 1);
	if (ret < 0)
		ak09973->SDRbit = 0;
	else
		ak09973->SDRbit = buf[0];

	ret = of_property_read_u32_array(np, "ak09973,SMR_setting", buf, 1);
	if (ret < 0)
		ak09973->SMRbit = 0;
	else
		ak09973->SMRbit = buf[0];

	ret = of_property_read_u32_array(np, "ak09973,threshold_X", buf, 2);
	if (ret < 0) {
		ak09973->BOPXbits = 0;
		ak09973->BRPXbits = 0;
	} else {
		ak09973->BOPXbits = buf[0];
		ak09973->BRPXbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ak09973,threshold_Y", buf, 2);
	if (ret < 0) {
		ak09973->BOPYbits = 0;
		ak09973->BRPYbits = 0;
	} else {
		ak09973->BOPYbits = buf[0];
		ak09973->BRPYbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ak09973,threshold_Z", buf, 2);
	if (ret < 0) {
		ak09973->BOPZbits = 0;
		ak09973->BRPZbits = 0;
	} else {
		ak09973->BOPZbits = buf[0];
		ak09973->BRPZbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ak09973,threshold_V", buf, 2);
	if (ret < 0) {
		ak09973->BOPVbits = 0;
		ak09973->BRPVbits = 0;
	} else {
		ak09973->BOPVbits = buf[0];
		ak09973->BRPVbits = buf[1];
	}

	ret = of_property_read_u32_array(np, "ak09973,SW_event", buf, 4);
	if (ret < 0) {
		ak09973->SWXENbit = 0;
		ak09973->SWYENbit = 0;
		ak09973->SWZENbit = 0;
		ak09973->SWVENbit = 0;
	} else {
		ak09973->SWXENbit = buf[0];
		ak09973->SWYENbit = buf[1];
		ak09973->SWZENbit = buf[2];
		ak09973->SWVENbit = buf[3];
	}

	return 0;
}


static int ak09973_set_trigger_state(struct iio_trigger *trig, bool state)
{

	pr_err("%s called. st=%s", __func__, (state ? "true" : "false"));

	return 0;
}


static const struct iio_trigger_ops ak09973_trigger_ops = {
	.set_trigger_state = ak09973_set_trigger_state,
};


static int ak09973_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ak09973_data *akm;
	struct iio_dev *indio_dev;
	struct device *dev = &client->dev;
	int err;
	const char *name = NULL;

	akdbgprt("[AK09973] %s(%d)\n", __func__, __LINE__);

	/* Register with IIO */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*akm));
	if (indio_dev == NULL)
		return -ENOMEM;

	akm = iio_priv(indio_dev);
	i2c_set_clientdata(client, indio_dev);

	akm->client = client;

	akm->numMode = ARRAY_SIZE(measurementFreqTable);

	akm->irq = 0;
	akm->int_gpio = of_get_gpio(client->dev.of_node, 0);
	if (gpio_is_valid(akm->int_gpio)) {
		err = devm_gpio_request_one(&client->dev, akm->int_gpio,
							GPIOF_IN, "ak09973_int");
		if (err < 0) {
			dev_err(&client->dev, "AK09973] failed to request GPIO %d, error %d\n",
							akm->int_gpio, err);
			return err;
		}

		akm->irq = gpio_to_irq(akm->int_gpio);
	}

	err = ak09973_parse_dt(akm);
	if (err < 0)
		dev_err(&client->dev, "[AK09973] Device Tree Setting was not found!\n");

	if (id)
		name = id->name;

	if (akm->irq) {
		akm->trig = iio_trigger_alloc("%s-dev%d", name, indio_dev->id);
		if (((akm->POLXbit == 1) && (akm->SWXENbit))
				|| ((akm->POLYbit == 1) && (akm->SWYENbit))
				|| ((akm->POLZbit == 1) && (akm->SWZENbit))
				|| ((akm->POLVbit == 1) && (akm->SWVENbit)))  {
			err = devm_request_irq(&client->dev, akm->irq,
							iio_trigger_generic_data_rdy_poll,
							IRQF_TRIGGER_RISING | IRQF_ONESHOT,
							dev_name(&client->dev), akm->trig);
			akdbgprt("[AK09973] %s Trigger : Rising\n", __func__);
		} else {
			err = devm_request_irq(&client->dev, akm->irq,
							iio_trigger_generic_data_rdy_poll,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							dev_name(&client->dev), akm->trig);
			akdbgprt("[AK09973] %s Trigger : Falling\n", __func__);
		}
		if (err)
			dev_err(&client->dev, "[AK09973] devm_request_irq Error! err=%d\n", err);

		akm->trig->dev.parent = dev;
		akm->trig->ops = &ak09973_trigger_ops;
		iio_trigger_set_drvdata(akm->trig, indio_dev);
		/* indio_dev->trig = iio_trigger_get(akm->trig); */
		err = iio_trigger_register(akm->trig);
		if (err)
			dev_err(&client->dev,
				"[AK09973] iio_trigger_register Error! err=%d\n", err);
		else
			indio_dev->trig = akm->trig;
	}

	akm->vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(akm->vdd))
		dev_err(&client->dev, "[AK09973] Failed to get VDD regulator\n");

	err = ak09973_setup(client);
	if (err < 0) {
		dev_err(&client->dev, "%s initialization fails\n", name);
		return err;
	}

	mutex_init(&akm->lock);
	indio_dev->dev.parent = &client->dev;
	indio_dev->channels = ak09973_channels;
	indio_dev->num_channels = ARRAY_SIZE(ak09973_channels);
	indio_dev->info = &ak09973_info;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->name = name;

	err = iio_triggered_buffer_setup(indio_dev, NULL, ak09973_handle_trigger,
					 NULL);
	if (err) {
		dev_err(&client->dev, "triggered buffer setup failed\n");
		return err;
	}
	akdbgprt("[AK09973] %s(iio_triggered_buffer_setup=%d)\n", __func__, err);

	err = iio_device_register(indio_dev);
	if (err)
		dev_err(&client->dev, "device register failed\n");

	akdbgprt("[AK09973] %s(iio_device_register=%d)\n", __func__, err);

	return err;

}

static int ak09973_remove(struct i2c_client *client)
{
	struct iio_dev *indio_dev = i2c_get_clientdata(client);
	struct ak09973_data *akm = iio_priv(indio_dev);

	iio_trigger_free(akm->trig);
	iio_device_unregister(indio_dev);
	iio_triggered_buffer_cleanup(indio_dev);

	ak09973_i2c_write8(client, AK09973_REG_CNTL2, 0);

	if (!IS_ERR(akm->vdd))
		 regulator_disable(akm->vdd);

	return 0;
}

static int ak09973_i2c_suspend(struct device *dev)
{
	struct ak09973_data *akm = iio_priv(i2c_get_clientdata(to_i2c_client(dev)));

	ak09973_i2c_write8(akm->client, AK09973_REG_CNTL2, 0);

	if (!IS_ERR(akm->vdd))
		regulator_disable(akm->vdd);

	return 0;
}

static int ak09973_i2c_resume(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);

	ak09973_setup(i2c);

	return 0;
}

static const struct dev_pm_ops ak09973_i2c_pops = {
	.suspend	= ak09973_i2c_suspend,
	.resume		= ak09973_i2c_resume,
};

static const struct i2c_device_id ak09973_id[] = {
	{ "ak09973", 0 },
	{ "ak09973_flip", 1 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, ak09973_id);

static const struct of_device_id ak09973_of_match[] = {
	{ .compatible = "akm,ak09973"},
	{ .compatible = "akm,ak09973_flip"},
	{}
};
MODULE_DEVICE_TABLE(of, ak09973_of_match);

static struct i2c_driver ak09973_driver = {
	.driver = {
		.name	= "ak09973",
		.pm = &ak09973_i2c_pops,
		.of_match_table = of_match_ptr(ak09973_of_match),
	},
	.probe		= ak09973_probe,
	.remove		= ak09973_remove,
	.id_table	= ak09973_id,
};
module_i2c_driver(ak09973_driver);

MODULE_AUTHOR("Junichi Wakasugi <wakasugi.jb@om.asahi-kasei.co.jp>");
MODULE_DESCRIPTION("AK09973 magnetometer driver");
MODULE_LICENSE("GPL v2");
