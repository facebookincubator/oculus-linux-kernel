// SPDX-License-Identifier: GPL-2.0
/*
 * IIO driver for PAC193x series chips
 *
 * Copyright (C) 2017 Microchip Technology Inc.
 *
 * Author: Bogdan Bolocan http://www.microchip.com/support
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/util_macros.h>
#include <linux/kthread.h>
#include <linux/i2c.h>
#include <linux/acpi.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/regulator/consumer.h>

#define PAC193X_MAX_RFSH_LIMIT						3600000
/* Overflow time = 248/(2n * SPS) secs
 * Where n is the number of bits in VPOWER.
 * n = 28 for Full Range minimum
 * n = 26 for Realistic minimum
 * VACCUM @ 1024Hz, Realistic min = 68.26 mins ~ 3600 secs
 */

#define PAC193X_MIN_POLLING_TIME					50
/* 50msec is the timeout for validity of the cached registers */

#define SHUNT_UOHMS_DEFAULT						100000

#define PAC193X_VOLTAGE_MILLIVOLTS_MAX					32000
/* 32000mV */
#define PAC193X_VOLTAGE_U_RES						16
/* voltage bits resolution when set for unsigned values */
#define PAC193X_VOLTAGE_S_RES						15
/* voltage bits resolution when set for signed values */

#define PAC193X_VSENSE_MILLIVOLTS_MAX					100
/* 100mV maximum for current shunts */
#define PAC193X_CURRENT_U_RES						16
/* voltage bits resolution when set for unsigned values */
#define PAC193X_CURRENT_S_RES						15
/* voltage bits resolution when set for signed values */

#define PAC193X_POWER_U_RES						28
/* power resolution is 28 bits when unsigned */
#define PAC193X_POWER_S_RES						27
/* power resolution is 27 bits when signed */

#define PAC193X_ENERGY_U_RES						48
/* energy accumulation is 48 bits long */
#define PAC193X_ENERGY_S_RES						47
#define PAC193X_ENERGY_SHIFT_MAIN_VAL					32

#define PAC193X_MAX_NUM_CHANNELS					4
#define PAC193X_MEAS_REG_SNAPSHOT_LEN					76
#define PAC193X_CTRL_REG_SNAPSHOT_LEN					12

#define PAC193x_CHIP_AVG_NUMBER						8

#define PAC193x_MIN_UPDATE_WAIT_TIME					1000
/* 1000usec is the minimum wait time for normal conversions when sample
 * rate doesn't change
 */
#define PAC193x_DEFAULT_CHIP_SAMP_SPEED					1024
#define PAC193x_SAMP_1024SPS						0
/* this sampling speed is represented as a 0b00 value */
#define PAC193x_SAMP_256SPS						1
#define PAC193x_SAMP_64SPS						2
#define PAC193x_SAMP_8SPS						3

/* I2C address map */
#define PAC193X_REFRESH_REG						0x00
#define PAC193X_CTRL_REG						0x01
#define PAC193X_REFRESH_V_REG						0x1F
#define PAC193X_ACC_COUNT_REG						0x02
#define PAC193X_CTRL_STAT_REGS_ADDR					0x1C
#define PAC193X_NEG_PWR_REGS_ADDR					0x1D
#define PAC193X_PID_REG_ADDR						0xFD

#define PAC193X_VPOWER_ACC_0_ADDR					0x03
#define PAC193X_VPOWER_ACC_1_ADDR					0x04
#define PAC193X_VPOWER_ACC_2_ADDR					0x05
#define PAC193X_VPOWER_ACC_3_ADDR					0x06
#define PAC193X_VBUS_0_ADDR						0x07
#define PAC193X_VBUS_1_ADDR						0x08
#define PAC193X_VBUS_2_ADDR						0x09
#define PAC193X_VBUS_3_ADDR						0x0A
#define PAC193X_VSENSE_0_ADDR						0x0B
#define PAC193X_VSENSE_1_ADDR						0x0C
#define PAC193X_VSENSE_2_ADDR						0x0D
#define PAC193X_VSENSE_3_ADDR						0x0E
#define PAC193X_VBUS_AVG_0_ADDR						0x0F
#define PAC193X_VBUS_AVG_1_ADDR						0x10
#define PAC193X_VBUS_AVG_2_ADDR						0x11
#define PAC193X_VBUS_AVG_3_ADDR						0x12
#define PAC193X_VSENSE_AVG_0_ADDR					0x13
#define PAC193X_VSENSE_AVG_1_ADDR					0x14
#define PAC193X_VSENSE_AVG_2_ADDR					0x15
#define PAC193X_VSENSE_AVG_3_ADDR					0x16
#define PAC193X_VPOWER_0_ADDR						0x17
#define PAC193X_VPOWER_1_ADDR						0x18
#define PAC193X_VPOWER_2_ADDR						0x19
#define PAC193X_VPOWER_3_ADDR						0x1A

/* these indexes are exactly describing the element order within a single
 * PAC193x phys channel IIO channel descriptor; see the static const struct
 * iio_chan_spec pac193x_single_channel[] declaration
 */
#define IIO_EN								0
#define IIO_POW								1
#define IIO_VOLT							2
#define IIO_CRT								3
#define IIO_VOLTAVG							4
#define IIO_CRTAVG							5

#define PAC193X_ACC_REG_LEN						3
#define PAC193X_VPOWER_ACC_REG_LEN					6
#define PAC193X_VBUS_SENSE_REG_LEN					2
#define PAC193X_VPOWER_REG_LEN						4

/* relative offsets when using multi-byte reads/writes even though these
 * bytes are read one after the other, they are not at adjacent memory
 * locations within the I2C memory map. The chip can skip some addresses
 */
#define PAC193X_CHANNEL_DIS_REG_OFF					0
#define PAC193X_NEG_PWR_REG_OFF						1
/* when reading/writing multiple bytes from offset PAC193X_CHANNEL_DIS_REG_OFF,
 * the chip jumps over the 0x1E (REFRESH_G) and 0x1F (REFRESH_V) offsets
 */
#define PAC193X_SLOW_REG_OFF						2
#define PAC193X_CTRL_ACT_REG_OFF					3
#define PAC193X_CHANNEL_DIS_ACT_REG_OFF					4
#define PAC193X_NEG_PWR_ACT_REG_OFF					5
#define PAC193X_CTRL_LAT_REG_OFF					6
#define PAC193X_CHANNEL_DIS_LAT_REG_OFF					7
#define PAC193X_NEG_PWR_LAT_REG_OFF					8
#define PAC193X_PID_REG_OFF						9
#define PAC193X_MID_REG_OFF						10
#define PAC193X_REV_REG_OFF						11

#define PAC193X_CTRL_STATUS_INFO_LEN		(PAC193X_REV_REG_OFF + 1)

#define PAC193X_CH_DIS_NOSKIP_VAL					0x02

#define PAC193X_MID							0x5D
#define PAC1934_PID							0x5B
#define PAC1932_PID							0x5C
#define PAC1931_PID							0x5D

#define CTRL_REG(samp, sleep, sing, al_p, al_cc, ovf_al) \
						((((u8)samp & 0x03) << 6) |\
						(((u8)sleep & 0x01) << 5) |\
						(((u8)sing & 0x01) << 4) |\
						(((u8)al_p & 0x01) << 3) |\
						(((u8)al_cc & 0x01) << 2) |\
						(((u8)ovf_al & 0x01) << 1))

#define CHANNEL_DIS_REG(ch1_on, ch2_on, ch3_on, ch4_on, \
			smb_tout, bycount, skip)\
						((ch1_on ? 0 : 0x80) | \
						(ch2_on ? 0 : 0x40) | \
						(ch3_on ? 0 : 0x20) | \
						(ch4_on ? 0 : 0x10) | \
					(((u8)smb_tout & 0x01) << 3) | \
					(((u8)bycount & 0x01) << 2) | \
						(skip ? 0 : 0x02))

#define NEG_PWR_REG(ch1_bidi, ch2_bidi, ch3_bidi, ch4_bidi, \
			ch1_bidv, ch2_bidv, ch3_bidv, ch4_bidv)\
						((ch1_bidi ? 0x80 : 0) | \
						(ch2_bidi ? 0x40 : 0) | \
						(ch3_bidi ? 0x20 : 0) | \
						(ch4_bidi ? 0x10 : 0) | \
						(ch1_bidv ? 0x08 : 0) | \
						(ch2_bidv ? 0x04 : 0) | \
						(ch3_bidv ? 0x02 : 0) | \
						(ch4_bidv ? 0x01 : 0))

enum pac193x_ids {
	pac1934,
	pac1932,
	pac1931
};

struct pac193x_features {
	u8 phys_channels;/*number of physical channels supported by the chip */
	u8 prod_id; /*product ID*/
};

struct samp_rate_mapping {
	u16 samp_rate;
	u8 shift2value;
};

static const u16 samp_rate_map_tbl[] = {
	[PAC193x_SAMP_1024SPS] = 1024,
	[PAC193x_SAMP_256SPS] = 256,
	[PAC193x_SAMP_64SPS] = 64,
	[PAC193x_SAMP_8SPS] = 8,
};

static const struct pac193x_features pac193x_chip_config[] = {
	[pac1934] = {
		.phys_channels = PAC193X_MAX_NUM_CHANNELS,
		.prod_id = PAC1934_PID,
	},
	[pac1932] = {
		.phys_channels = PAC193X_MAX_NUM_CHANNELS >> 1,
		.prod_id = PAC1932_PID,
	},
	[pac1931] = {
		.phys_channels = PAC193X_MAX_NUM_CHANNELS >> 2,
		.prod_id = PAC1931_PID,
	},
};

struct reg_data {
	/* these fields keep track of chip's runtime configuration */
	bool	active_channels[PAC193X_MAX_NUM_CHANNELS];
	bool	bi_dir[PAC193X_MAX_NUM_CHANNELS];

	/* these variables keep the chip values as read from it - snapshot */
	u8	meas_regs[PAC193X_MEAS_REG_SNAPSHOT_LEN];
	u8	ctrl_regs[PAC193X_CTRL_REG_SNAPSHOT_LEN];
	u32	acc_count;
	s64	energy_sec_acc[PAC193X_MAX_NUM_CHANNELS];
	s64	vpower_acc[PAC193X_MAX_NUM_CHANNELS];
	s32	vpower[PAC193X_MAX_NUM_CHANNELS];
	s32	vbus[PAC193X_MAX_NUM_CHANNELS];
	s32	vbus_avg[PAC193X_MAX_NUM_CHANNELS];
	s32	vsense[PAC193X_MAX_NUM_CHANNELS];
	s32	vsense_avg[PAC193X_MAX_NUM_CHANNELS];
	unsigned long jiffies_tstamp;
	/* this variable stores the current sampling speed */
	u8	crt_samp_speed_bitfield;
	/* this variable keeps the count of how many chip
	 * channels are currently enabled
	 */
	u8	num_enabled_channels;
	uint64_t tstamp_ms;
};
struct pac193x_chip_info {
	const struct iio_chan_spec	*channels;
	const struct iio_info		*indio_info;
	struct i2c_client		*client;
	struct mutex			lock;
	struct regulator		*avdd_reg;

	struct timer_list		tmr_forced_update;
	/* to be used to now when will be the chip read timeout */
	u32				forced_reads_triggered;
	u32				rearm_force_read;

	/* workqueue for periodic chip readings to prevent saturation */
	struct workqueue_struct		*wq_chip;
	struct work_struct		work_chip_rfsh;

	u8				phys_channels;
	u8				chip_variant;
	u8				chip_revision;

	u32				shunts[PAC193X_MAX_NUM_CHANNELS];
	const char			*rail_name[PAC193X_MAX_NUM_CHANNELS];
	const char			*subsys_name[PAC193X_MAX_NUM_CHANNELS];
	struct reg_data			chip_reg_data;
	unsigned int			avg_num;
	u32				sample_rate_value;
};

#define to_pac193x_chip_info(d) container_of(d, struct pac193x_chip_info, \
						work_chip_rfsh)
/* macros to extract the parameters */
#define mACC_COUNT(addr)		(((u32)(*(u8 *)(addr + 0)) << 16) | \
					((u32)(*(u8 *)(addr + 1)) << 8) | \
					((u32)(*(u8 *)(addr + 2)) << 0))

#define mVPOWER_ACCu(addr)		(((u64)(*(u8 *)(addr + 0)) << 40) | \
					((u64)(*(u8 *)(addr + 1)) << 32) | \
					((u64)(*(u8 *)(addr + 2)) << 24) | \
					((u64)(*(u8 *)(addr + 3)) << 16) | \
					((u64)(*(u8 *)(addr + 4)) << 8) | \
					((u64)(*(u8 *)(addr + 5)) << 0))

#define mVPOWER_ACCs(addr)		sign_extend64(mVPOWER_ACCu(addr), 47)

#define mVPOWERu(addr)			(((u32)(*(u8 *)(addr + 0)) << 20) | \
					((u32)(*(u8 *)(addr + 1)) << 12) | \
					((u32)(*(u8 *)(addr + 2)) << 4) | \
					((u32)(*(u8 *)(addr + 3)) >> 4))

#define mVPOWERs(addr)			sign_extend32(mVPOWERu(addr), 3)

#define mVBUS_SENSEu(addr)		(((u16)(*(u8 *)(addr + 0)) << 8) | \
					((u16)(*(u8 *)(addr + 1)) << 0))

#define mVBUS_SENSEs(addr)		((__s16)mVBUS_SENSEu(addr))

static int pac193x_retrieve_data(struct pac193x_chip_info *chip_info,
					u32 wait_time);
static int pac193x_send_rfsh(struct pac193x_chip_info *chip_info,
				bool refresh_v, u32 wait_time);
static int pac193x_reg_snapshot(struct pac193x_chip_info *chip_info,
				bool do_rfsh, bool refresh_v, u32 wait_time);
static int pac193x_remove(struct i2c_client *client);
static const char *pac193x_get_of_match_entry(struct i2c_client *client);

#define PAC193x_VPOWER_ACC_CHANNEL(_index, _address) {			\
	.type = IIO_ENERGY,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_AVERAGE_RAW) |		\
				BIT(IIO_CHAN_INFO_SCALE),		\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_ENERGY_U_RES,			\
		.storagebits = PAC193X_ENERGY_U_RES,			\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_VBUS_CHANNEL(_index, _address) {			\
	.type = IIO_VOLTAGE,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE),		\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_VOLTAGE_U_RES,			\
		.storagebits = PAC193X_VOLTAGE_U_RES,			\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_VBUS_AVG_CHANNEL(_index, _address) {			\
	.type = IIO_VOLTAGE,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_AVERAGE_RAW),		\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) |		\
				BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),	\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_VOLTAGE_U_RES,			\
		.storagebits = PAC193X_VOLTAGE_U_RES,			\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_VSENSE_CHANNEL(_index, _address) {			\
	.type = IIO_CURRENT,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),			\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_CURRENT_U_RES,			\
		.storagebits = PAC193X_CURRENT_U_RES,			\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_VSENSE_AVG_CHANNEL(_index, _address) {			\
	.type = IIO_CURRENT,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_AVERAGE_RAW) |		\
			BIT(IIO_CHAN_INFO_SCALE),			\
	.info_mask_shared_by_type = BIT(IIO_CHAN_INFO_OVERSAMPLING_RATIO),\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_CURRENT_U_RES,			\
		.storagebits = PAC193X_CURRENT_U_RES,			\
		.shift = 0,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_VPOWER_CHANNEL(_index, _address) {			\
	.type = IIO_POWER,						\
	.address = (_address),						\
	.indexed = 1,							\
	.channel = (_index),						\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |			\
			BIT(IIO_CHAN_INFO_SCALE),			\
	.info_mask_shared_by_dir = BIT(IIO_CHAN_INFO_SAMP_FREQ),	\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 'u',						\
		.realbits = PAC193X_POWER_U_RES,			\
		.storagebits = 32,					\
		.shift = 4,						\
		.endianness = IIO_CPU,					\
	}								\
}

#define PAC193x_SOFT_TIMESTAMP(_index) {				\
	.type = IIO_TIMESTAMP,						\
	.channel = -1,							\
	.scan_index = (_index),						\
	.scan_type = {							\
		.sign = 's',						\
		.realbits = 64,						\
		.storagebits = 64,					\
		}							\
}

static const struct iio_chan_spec pac193x_single_channel[] = {
	PAC193x_VPOWER_ACC_CHANNEL(0, PAC193X_VPOWER_ACC_0_ADDR),
	PAC193x_VPOWER_CHANNEL(0, PAC193X_VPOWER_0_ADDR),
	PAC193x_VBUS_CHANNEL(0, PAC193X_VBUS_0_ADDR),
	PAC193x_VSENSE_CHANNEL(0, PAC193X_VSENSE_0_ADDR),
	PAC193x_VBUS_AVG_CHANNEL(0, PAC193X_VBUS_AVG_0_ADDR),
	PAC193x_VSENSE_AVG_CHANNEL(0, PAC193X_VSENSE_AVG_0_ADDR),
};

static const struct iio_chan_spec pac193x_ts[] = {
	PAC193x_SOFT_TIMESTAMP(0),
};

/* Low-level I2c functions */
static int pac193x_i2c_read(struct i2c_client *client, u8 reg_addr,
				void *databuf, u8 len)
{
	int ret;
	struct i2c_msg msgs[2] = {
		{ .addr = client->addr, .len = 1,
			.buf = (u8 *) &reg_addr, .flags = 0 },
		{ .addr = client->addr, .len = len,
			.buf = databuf, .flags = I2C_M_RD } };

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_err("failed reading data from register 0x%02X\n", reg_addr);
		return ret;
	}
	return 0;
}

static int pac193x_i2c_write_byte(struct i2c_client *client,
					u8 reg_addr, u8 val)
{
	int ret;
	u8 buf[2];
	struct i2c_msg msgs[1] = {
		{ .addr = client->addr, .len = sizeof(buf),
			.buf = (u8 *) &buf, .flags = 0 } };
	buf[0] = reg_addr;
	buf[1] = val;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_err("failed writing register 0x%02X\n", reg_addr);
		return ret;
	}
	return 0;
}

static int pac193x_i2c_send_byte(struct i2c_client *client, u8 reg_addr)
{
	int ret;
	u8 buf;
	struct i2c_msg msgs[1] = {
		{ .addr = client->addr, .len = sizeof(buf),
			.buf = (u8 *) &buf, .flags = 0 } };
	buf = reg_addr;

	ret = i2c_transfer(client->adapter, msgs, ARRAY_SIZE(msgs));
	if (ret < 0) {
		pr_err("failed sending byte to register 0x%02X\n", reg_addr);
		return ret;
	}
	return 0;
}

static int pac193x_i2c_write(struct i2c_client *client, u8 reg_addr,
				int len, u8 *data)
{
	int ret;
	u8 send[len + 1];
	struct i2c_msg msg = { .addr = client->addr,
				.len = len + 1, .flags = 0 };

	send[0] = reg_addr;
	memcpy(&send[1], data, len * sizeof(u8));
	msg.buf = send;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret < 0) {
		pr_err("failed writing data from register 0x%02X\n", reg_addr);
		return ret;
	}
	return 0;
}

static int pac193x_match_samp_rate(struct pac193x_chip_info *chip_info,
					u32 new_samp_rate)
{
	int cnt;

	for (cnt = 0; cnt < ARRAY_SIZE(samp_rate_map_tbl); cnt++) {
		if (new_samp_rate == samp_rate_map_tbl[cnt]) {
			chip_info->chip_reg_data.crt_samp_speed_bitfield = cnt;
			break;
		}
	}
	if (cnt == ARRAY_SIZE(samp_rate_map_tbl)) {
		/* not a valid sample rate value */
		return cnt;
	}
	return 0;
}

static ssize_t rst_en_regs_wo_param_store
			(struct device *dev, struct device_attribute *attr,
			 const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int val;
	int cnt;

	if (kstrtoint(buf, 10, &val))
		return -EINVAL;
	if (!(val == 0 || val == 1))
		return -EINVAL;

	mutex_lock(&chip_info->lock);
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++)
		chip_info->chip_reg_data.energy_sec_acc[cnt] = 0;
	mutex_unlock(&chip_info->lock);
	return count;
}


static ssize_t available_rails_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int len = 0;
	int cnt;

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		len += scnprintf(buf + len, PAGE_SIZE - len, "%s:%s\n",
				 chip_info->rail_name[cnt],
				 chip_info->subsys_name[cnt]);
	}
	return len;
}


static ssize_t enabled_rails_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int len = 0;
	int cnt;

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		if (!chip_info->chip_reg_data.active_channels[cnt])
			continue;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%s:%s\n",
				 chip_info->rail_name[cnt],
				 chip_info->subsys_name[cnt]);
	}
	return len;
}

static ssize_t average_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	const char *ptr;
	unsigned long temp;
	unsigned int voltage_value;
	unsigned int current_value;
	unsigned int bit_resolution;
	int len = 0;
	int cnt;
	int ret;

	/* Refresh registers */
	ret = pac193x_retrieve_data(chip_info, PAC193x_MIN_UPDATE_WAIT_TIME);
	if (ret < 0) {
		pr_err("unable to retrieve average data\n");
		return ret;
	}

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		if (!chip_info->chip_reg_data.active_channels[cnt])
			continue;

		if (chip_info->chip_reg_data.bi_dir[cnt])
			bit_resolution = PAC193X_VOLTAGE_S_RES;
		else
			bit_resolution = PAC193X_VOLTAGE_U_RES;

		ptr = chip_info->rail_name[cnt];

		/* Scale voltage register value to uV */
		temp = (unsigned long)
		       (chip_info->chip_reg_data.vbus_avg[cnt] *
			PAC193X_VOLTAGE_MILLIVOLTS_MAX) * 1000;
		voltage_value = temp >> bit_resolution;

		/* Scale current register value to uV */
		temp = PAC193X_VSENSE_MILLIVOLTS_MAX;
		temp /= chip_info->shunts[cnt] / 1000;
		temp *= chip_info->chip_reg_data.vsense_avg[cnt];
		temp *= 1000000;
		current_value = temp >> bit_resolution;

		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "%s, %d, %d\n",
				 ptr, voltage_value, current_value);
	}

	return len;
}

static ssize_t inst_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	const char *ptr;
	unsigned long temp;
	unsigned int voltage_value;
	unsigned int current_value;
	unsigned int power_value;
	unsigned int bit_resolution;
	unsigned int bit_resolution_power;
	int len = 0;
	int cnt;
	int ret;

	/* Refresh registers */
	ret = pac193x_retrieve_data(chip_info, PAC193x_MIN_UPDATE_WAIT_TIME);
	if (ret < 0) {
		pr_err("unable to retrieve inst data\n");
		return ret;
	}
	len += scnprintf(buf + len, PAGE_SIZE - len, "%lu\n",
			 chip_info->chip_reg_data.tstamp_ms);

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		if (!chip_info->chip_reg_data.active_channels[cnt])
			continue;

		if (chip_info->chip_reg_data.bi_dir[cnt]) {
			bit_resolution = PAC193X_VOLTAGE_S_RES;
			bit_resolution_power = PAC193X_POWER_S_RES;
		} else {
			bit_resolution = PAC193X_VOLTAGE_U_RES;
			bit_resolution_power = PAC193X_POWER_U_RES;
		}

		ptr = chip_info->rail_name[cnt];

		/* Scale voltage register value to uV */
		temp = (unsigned long)
			(chip_info->chip_reg_data.vbus[cnt] *
			 PAC193X_VOLTAGE_MILLIVOLTS_MAX) * 1000;
		voltage_value = temp >> bit_resolution;

		/* Scale current register value to uV */
		temp = PAC193X_VSENSE_MILLIVOLTS_MAX;
		temp /= chip_info->shunts[cnt] / 1000;
		temp *= chip_info->chip_reg_data.vsense[cnt];
		temp *= 1000000;
		current_value = temp >> bit_resolution;

		/* convert resistor value from uOhm to mOhm */
		temp = chip_info->shunts[cnt] / 1000;

		/* Scale register value to uW */
		temp = PAC193X_VSENSE_MILLIVOLTS_MAX;
		temp /= chip_info->shunts[cnt] / 1000;
		temp *= PAC193X_VOLTAGE_MILLIVOLTS_MAX * 1000;
		temp *= chip_info->chip_reg_data.vpower[cnt];
		power_value = temp >> bit_resolution_power;

		len += scnprintf(buf + len, PAGE_SIZE - len,
				 "%s, %d, %d, %d\n",
				 ptr, voltage_value,
				 current_value, power_value);
	}
	return len;
}

static ssize_t energy_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	const char *ptr;
	unsigned long temp;
	unsigned int bit_resolution;
	int len = 0;
	int cnt;
	int ret;

	/* Refresh registers */
	ret = pac193x_retrieve_data(chip_info, PAC193x_MIN_UPDATE_WAIT_TIME);
	if (ret < 0) {
		pr_err("unable to retrieve energy\n");
		return ret;
	}

	/* get time since boot in ms */
	len += scnprintf(buf + len, PAGE_SIZE - len, "%lu\n",
			 chip_info->chip_reg_data.tstamp_ms);

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		u64 a;
		u64 b;

		if (!chip_info->chip_reg_data.active_channels[cnt])
			continue;

		if (chip_info->chip_reg_data.bi_dir[cnt])
			bit_resolution = PAC193X_POWER_S_RES;
		else
			bit_resolution = PAC193X_POWER_U_RES;

		ptr = chip_info->rail_name[cnt];

		/* Scale register value to uW-secs */
		a = PAC193X_VSENSE_MILLIVOLTS_MAX;
		a /= chip_info->shunts[cnt] / 1000;
		a *= PAC193X_VOLTAGE_MILLIVOLTS_MAX * 1000;
		b = chip_info->chip_reg_data.energy_sec_acc[cnt];
		temp = mul_u64_u64_shr(a, b, bit_resolution);

		/* Convert to uW-secs */
		temp /= chip_info->sample_rate_value;

		len += scnprintf(buf + len, PAGE_SIZE - len, "%s, %lu\n",
				 ptr,
				 temp);
	}
	return len;
}

static ssize_t sampling_rate_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			(int)chip_info->sample_rate_value);
}

static ssize_t sampling_rate_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	unsigned int old_sampling_rate;
	int sampling_rate;
	int ret;

	if (kstrtoint(buf, 10, &sampling_rate)) {
		pr_err("sampling_rate is not a number\n");
		return -EINVAL;
	}
	if (pac193x_match_samp_rate(chip_info, (u16)sampling_rate)) {
		pr_err("sampling_rate invalid. Must be %d, %d, %d, %d\n",
			samp_rate_map_tbl[PAC193x_SAMP_8SPS],
			samp_rate_map_tbl[PAC193x_SAMP_64SPS],
			samp_rate_map_tbl[PAC193x_SAMP_256SPS],
			samp_rate_map_tbl[PAC193x_SAMP_1024SPS]);
		return -EINVAL;
	}

	/* store the old sampling rate */
	old_sampling_rate = chip_info->sample_rate_value;

	/* we have a valid sample rate */
	chip_info->sample_rate_value = (u16)sampling_rate;

	/* now lock the access to the chip, write the new
	 * sampling value and trigger a snapshot(incl refresh)
	 */
	mutex_lock(&chip_info->lock);

	/* enable ALERT pin */
	ret = pac193x_i2c_write_byte(chip_info->client,
		PAC193X_CTRL_REG,
		CTRL_REG(chip_info->chip_reg_data.crt_samp_speed_bitfield,
			 0, 0, 1, 0, 0));
	if (ret < 0) {
		pr_err("cannot write ctrl reg at 0x%02X\n", PAC193X_CTRL_REG);
		/* revert back to old value */
		chip_info->sample_rate_value = old_sampling_rate;
		pac193x_match_samp_rate(chip_info, (u16)old_sampling_rate);
		mutex_unlock(&chip_info->lock);
		return ret;
	}

	/* unlock the access towards the chip - register
	 * snapshot includes its own access lock
	 */
	mutex_unlock(&chip_info->lock);

	/* now, force a snapshot with refresh - call retrieve
	 * data in order to update the refresh timer
	 * alter the timestamp in order to force trigger a
	 * register snapshot and a timestamp update
	 */
	chip_info->chip_reg_data.jiffies_tstamp -=
		msecs_to_jiffies(PAC193X_MIN_POLLING_TIME);
	ret = pac193x_retrieve_data(chip_info,
				    (1024 / old_sampling_rate) * 1000);
	if (ret < 0) {
		pr_err("cannot snapshot regs\n");
		return ret;
	}

	return count;
}


static ssize_t shunt_value_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	unsigned int i;
	int len = 0;
	int cnt;

	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		i = chip_info->shunts[cnt];
		len += scnprintf(buf + len, PAGE_SIZE - len, "%d ", i);
	}

	if (len >= 1)
		buf[len - 1] = '\n';

	return len;
}

static ssize_t shunt_value_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int chan, sh_val;
	char *blank, mybuff[8];

	blank = strnchr(buf, ' ', count);
	if (!blank) {
		pr_err("shunt_value: Missing parameters\n");
		return -EINVAL;
	}
	memset(mybuff, 0, sizeof(mybuff));
	memcpy(mybuff, buf, blank - buf);
	if (kstrtoint(mybuff, 10, &chan)) {
		pr_err("shunt_value: Channel index is not a number\n");
		return -EINVAL;
	}
	if (chan < 0) {
		pr_err("shunt_value: Negative channel not allowed\n");
		return -EINVAL;
	}
	if (chan >= chip_info->phys_channels) {
		pr_err("shunt_value: Channel index out of range\n");
		return -EINVAL;
	}
	if (kstrtoint(++blank, 10, &sh_val)) {
		pr_err("shunt_value: Shunt value is not a number\n");
		return -EINVAL;
	}
	if (sh_val < 0) {
		pr_err("shunt_value: Negative shunt values not allowed\n");
		return -EINVAL;
	}
	mutex_lock(&chip_info->lock);
	chip_info->shunts[chan] = sh_val;
	mutex_unlock(&chip_info->lock);
	return count;
}

static IIO_DEVICE_ATTR(rst_en_regs_wo_param, 0200,
		       NULL, rst_en_regs_wo_param_store, 0);

static IIO_DEVICE_ATTR(available_rails, 0444,
		       available_rails_show,
		       NULL, 0);

static IIO_DEVICE_ATTR(enabled_rails, 0444,
		       enabled_rails_show,
		       NULL, 0);

static IIO_DEVICE_ATTR(average_value, 0444,
		       average_value_show,
		       NULL, 0);

static IIO_DEVICE_ATTR(inst_value, 0444,
		       inst_value_show,
		       NULL, 0);

static IIO_DEVICE_ATTR(sampling_rate, 0644,
		       sampling_rate_show,
		       sampling_rate_store, 0);

static IIO_DEVICE_ATTR(shunt_value, 0644,
		       shunt_value_show,
		       shunt_value_store, 0);

static IIO_DEVICE_ATTR(energy_value, 0444,
		       energy_value_show,
		       NULL, 0);


#define PAC193x_DEV_ATTR(name) (&iio_dev_attr_##name.dev_attr.attr)

static struct attribute *pac193x_custom_attributes[] = {
	PAC193x_DEV_ATTR(rst_en_regs_wo_param),
	PAC193x_DEV_ATTR(available_rails),
	PAC193x_DEV_ATTR(enabled_rails),
	PAC193x_DEV_ATTR(average_value),
	PAC193x_DEV_ATTR(inst_value),
	PAC193x_DEV_ATTR(sampling_rate),
	PAC193x_DEV_ATTR(shunt_value),
	PAC193x_DEV_ATTR(energy_value),
	NULL
};

static const struct attribute_group pac193x_group = {
	.attrs = pac193x_custom_attributes,
};
/*
 * pac193x_read_raw() - data read function.
 * @indio_dev:	the struct iio_dev associated with this device instance
 * @chan:	the channel whose data is to be read
 * @val:	first element of returned value (typically INT)
 * @val2:	second element of returned value (typically MICRO)
 * @mask:	what we actually want to read as per the info_mask_*
 *		in iio_chan_spec.
 */
static int pac193x_read_raw(struct iio_dev *indio_dev,
			      struct iio_chan_spec const *chan,
			      int *val,
			      int *val2,
			      long mask)
{
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int ret = -EINVAL;
	unsigned long tmp;

	ret = pac193x_retrieve_data(chip_info, PAC193x_MIN_UPDATE_WAIT_TIME);
	if (ret < 0)
		return ret;
	/* check what data is requested from us */
	ret = -EINVAL;

	switch (mask) {
	/* Raw data requested */
	case IIO_CHAN_INFO_RAW:
		switch (chan->type) {
		/* Voltages */
		case IIO_VOLTAGE:
			switch (chan->address) {
			case PAC193X_VBUS_0_ADDR:
			case PAC193X_VBUS_1_ADDR:
			case PAC193X_VBUS_2_ADDR:
			case PAC193X_VBUS_3_ADDR:
				*val =
				chip_info->chip_reg_data.vbus[chan->channel];
				return IIO_VAL_INT;

			default:
				return -EINVAL;
			}
			break;
		/* Currents */
		case IIO_CURRENT:
			switch (chan->address) {
			case PAC193X_VSENSE_0_ADDR:
			case PAC193X_VSENSE_1_ADDR:
			case PAC193X_VSENSE_2_ADDR:
			case PAC193X_VSENSE_3_ADDR:
				*val =
				chip_info->chip_reg_data.vsense[chan->channel];
				return IIO_VAL_INT;
			default:
				return -EINVAL;
			}
			break;
		/* Power */
		case IIO_POWER:
			switch (chan->address) {
			case PAC193X_VPOWER_0_ADDR:
			case PAC193X_VPOWER_1_ADDR:
			case PAC193X_VPOWER_2_ADDR:
			case PAC193X_VPOWER_3_ADDR:
				*val =
				chip_info->chip_reg_data.vpower[chan->channel];
				return IIO_VAL_INT;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	/* Average raw data */
	case IIO_CHAN_INFO_AVERAGE_RAW:
		switch (chan->type) {
		/* Voltages */
		case IIO_VOLTAGE:
			switch (chan->address) {
			case PAC193X_VBUS_AVG_0_ADDR:
			case PAC193X_VBUS_AVG_1_ADDR:
			case PAC193X_VBUS_AVG_2_ADDR:
			case PAC193X_VBUS_AVG_3_ADDR:
				*val =
			chip_info->chip_reg_data.vbus_avg[chan->channel];
				return IIO_VAL_INT;

			default:
				return -EINVAL;
			}
			break;
		/* Currents */
		case IIO_CURRENT:
			switch (chan->address) {
			case PAC193X_VSENSE_AVG_0_ADDR:
			case PAC193X_VSENSE_AVG_1_ADDR:
			case PAC193X_VSENSE_AVG_2_ADDR:
			case PAC193X_VSENSE_AVG_3_ADDR:
				*val =
			chip_info->chip_reg_data.vsense_avg[chan->channel];
				return IIO_VAL_INT;

			default:
				return -EINVAL;
			}
			break;
		/* Energy */
		case IIO_ENERGY:
			switch (chan->address) {
			case PAC193X_VPOWER_ACC_0_ADDR:
			case PAC193X_VPOWER_ACC_1_ADDR:
			case PAC193X_VPOWER_ACC_2_ADDR:
			case PAC193X_VPOWER_ACC_3_ADDR:
	/* reduce the value only to the higher 32 bits in order to fit the val
	 * parameter which is int(32b)
	 */
				*val =
(int)(chip_info->chip_reg_data.energy_sec_acc[chan->channel] >>
PAC193X_ENERGY_SHIFT_MAIN_VAL);
				return IIO_VAL_INT;
			default:
				return -EINVAL;
			}
			break;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_SCALE:
		switch (chan->address) {
		/* Voltages - scale for millivolts */
		case PAC193X_VBUS_0_ADDR:
		case PAC193X_VBUS_1_ADDR:
		case PAC193X_VBUS_2_ADDR:
		case PAC193X_VBUS_3_ADDR:
		case PAC193X_VBUS_AVG_0_ADDR:
		case PAC193X_VBUS_AVG_1_ADDR:
		case PAC193X_VBUS_AVG_2_ADDR:
		case PAC193X_VBUS_AVG_3_ADDR:
			*val = PAC193X_VOLTAGE_MILLIVOLTS_MAX;
			if (chan->scan_type.sign == 'u')
				*val2 = PAC193X_VOLTAGE_U_RES;
			else
				*val2 = PAC193X_VOLTAGE_S_RES;
			return IIO_VAL_FRACTIONAL_LOG2;
		/* Currents - scale for mA - depends on the
		 * channel's shunt value
		 * ( 100mV * 1000000) / (2^16 * shunt(uohm))
		 */
		case PAC193X_VSENSE_0_ADDR:
		case PAC193X_VSENSE_1_ADDR:
		case PAC193X_VSENSE_2_ADDR:
		case PAC193X_VSENSE_3_ADDR:
		case PAC193X_VSENSE_AVG_0_ADDR:
		case PAC193X_VSENSE_AVG_1_ADDR:
		case PAC193X_VSENSE_AVG_2_ADDR:
		case PAC193X_VSENSE_AVG_3_ADDR:
			tmp = (PAC193X_VSENSE_MILLIVOLTS_MAX * 1000000)/
				chip_info->shunts[chan->channel];
			*val = (int)tmp;
			if (chan->scan_type.sign == 'u')
				*val2 = PAC193X_CURRENT_U_RES;
			else
				*val2 = PAC193X_CURRENT_S_RES;
			return IIO_VAL_FRACTIONAL_LOG2;
		/* Power - mW - it will use the combined scale
		 * for current and voltage
		 * current(mA) * voltage(mV) = power (uW)
		 */
		case PAC193X_VPOWER_0_ADDR:
		case PAC193X_VPOWER_1_ADDR:
		case PAC193X_VPOWER_2_ADDR:
		case PAC193X_VPOWER_3_ADDR:
			tmp = (PAC193X_VSENSE_MILLIVOLTS_MAX) * 100 *
				((10000 * PAC193X_VOLTAGE_MILLIVOLTS_MAX)/
					chip_info->shunts[chan->channel]);
			*val = (int)tmp;
			if (chan->scan_type.sign == 'u')
				*val2 = PAC193X_POWER_U_RES;
			else
				*val2 = PAC193X_POWER_S_RES;
			return IIO_VAL_FRACTIONAL_LOG2;
		case PAC193X_VPOWER_ACC_0_ADDR:
		case PAC193X_VPOWER_ACC_1_ADDR:
		case PAC193X_VPOWER_ACC_2_ADDR:
		case PAC193X_VPOWER_ACC_3_ADDR:
		/* compute the scale for energy (Watt-second or
		 * Joule). The Energy mean raw value was the
		 * higher 32 bits of the s64 variable. The right
		 * shifted raw value was needed in order to
		 * prevent the underflow of the scale value.
		 * Both the raw and the scale are accepting only
		 * signed 32bits values
		 */
			tmp = (PAC193X_VSENSE_MILLIVOLTS_MAX *
				PAC193X_VOLTAGE_MILLIVOLTS_MAX)/
				chip_info->shunts[chan->channel];
			*val = (int)tmp;
			if (chan->scan_type.sign == 'u')
				*val2 = PAC193X_ENERGY_U_RES -
					PAC193X_ENERGY_SHIFT_MAIN_VAL;
			else
				*val2 = PAC193X_ENERGY_S_RES -
					PAC193X_ENERGY_SHIFT_MAIN_VAL;
			return IIO_VAL_FRACTIONAL_LOG2;
		default:
			return -EINVAL;
		}
		break;
	case IIO_CHAN_INFO_SAMP_FREQ:
		*val = (int)chip_info->sample_rate_value;
		return IIO_VAL_INT;
	default:
		return -EINVAL;
	}
	return ret;
}
/*
 * pac193x_write_raw() - data write function.
 * @indio_dev:	the struct iio_dev associated with this device instance
 * @chan:	the channel whose data is to be written
 * @val:	first element of value to set (typically INT)
 * @val2:	second element of value to set (typically MICRO)
 * @mask:	what we actually want to write as per the info_mask_*
 *		in iio_chan_spec.
 *
 * Note that all raw writes are assumed IIO_VAL_INT and info mask elements
 * are assumed to be IIO_INT_PLUS_MICRO unless the callback write_raw_get_fmt
 * in struct iio_info is provided by the driver.
 */
static int pac193x_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	int ret = -EINVAL;
	u32 old_samp_rate;

	if (iio_buffer_enabled(indio_dev))
		return -EBUSY;

	switch (mask) {
	case IIO_CHAN_INFO_SAMP_FREQ:
		if (pac193x_match_samp_rate(chip_info, (u16)val))
			return -EINVAL;
		/* store the old sampling rate */
		old_samp_rate = chip_info->sample_rate_value;
		/* we have a valid sample rate */
		chip_info->sample_rate_value = (u16)val;
		/* now lock the access to the chip, write the new
		 * sampling value and trigger a snapshot(incl refresh)
		 */
		mutex_lock(&chip_info->lock);
		/* enable ALERT pin */
		ret = pac193x_i2c_write_byte(chip_info->client,
			PAC193X_CTRL_REG,
		CTRL_REG(chip_info->chip_reg_data.crt_samp_speed_bitfield,
		0, 0, 1, 0, 0));
		if (ret < 0) {
			pr_err("cannot write ctrl reg at 0x%02X\n",
				PAC193X_CTRL_REG);
			mutex_unlock(&chip_info->lock);
			return ret;
		}
		/* unlock the access towards the chip - register
		 * snapshot includes its own access lock
		 */
		mutex_unlock(&chip_info->lock);
		/* now, force a snapshot with refresh - call retrieve
		 * data in order to update the refresh timer
		 * alter the timestamp in order to force trigger a
		 * register snapshot and a timestamp update
		 */
		chip_info->chip_reg_data.jiffies_tstamp -=
				msecs_to_jiffies(PAC193X_MIN_POLLING_TIME);
		ret = pac193x_retrieve_data(chip_info,
				(1024/old_samp_rate) * 1000);
		if (ret < 0) {
			pr_err("cannot snapshot regs\n");
			return ret;
		}
		ret = 0;
	break;
	}
	return ret;
}

static const struct iio_info pac193x_info = {
	.attrs = &pac193x_group,
	.read_raw = pac193x_read_raw,
	.write_raw = pac193x_write_raw,
};

static int pac193x_send_rfsh(struct pac193x_chip_info *chip_info,
				bool refresh_v, u32 wait_time)
{
	/* this function only sends REFRESH or REFRESH_V */
	int ret;
	u8 rfsh_option, reg = 0;
	/* if refresh_v is not false, send a REFRESH_V instead
	 * (doesn't reset the accumulators)
	 */
	rfsh_option = PAC193X_REFRESH_REG;
	if (refresh_v)
		rfsh_option = PAC193X_REFRESH_V_REG;

	/* SW WAR for reducing high supply current
	 * An anomaly has been noted with this family of devices. When a
	 * REFRESH command (SEND Byte) is followed directly by another
	 * REFRESH command (SEND Byte), the internal clock does not stop
	 * on schedule. This causes higher supply current because the
	 * device is not fully in SLEEP mode between conversions as it
	 * should be.
	 * A workaround for the anomaly is to WRITE to any R/W register
	 * before and after each REFRESH command. This causes the clock
	 * to stop between sampling cycles as it should. This workaround
	 * produces no side effects except for the burden of the extra
	 * WRITE commands.
	 */
	ret = pac193x_i2c_write(chip_info->client, PAC193X_NEG_PWR_REGS_ADDR,
				1, &reg);
	if (ret < 0) {
		pr_err("cannot write to reg:0x%02X\n",
		       PAC193X_NEG_PWR_REGS_ADDR);
		return ret;
	}
	/* now write a REFRESH or a REFRESH_V command */
	ret = pac193x_i2c_send_byte(chip_info->client, rfsh_option);
	if (ret < 0) {
		pr_err("cannot send byte 0x%02X reg\n", rfsh_option);
		return ret;
	}
	/* SW WAR for reducing high supply current */
	ret = pac193x_i2c_write(chip_info->client, PAC193X_NEG_PWR_REGS_ADDR,
				1, &reg);
	if (ret < 0) {
		pr_err("cannot write to reg:0x%02X\n",
		       PAC193X_NEG_PWR_REGS_ADDR);
		return ret;
	}
	/* register data retrieval timestamp */
	chip_info->chip_reg_data.jiffies_tstamp = jiffies;
	/* wait till the data is available */
	usleep_range(wait_time, wait_time + 100);
	return ret;
}

static int pac193x_reg_snapshot(struct pac193x_chip_info *chip_info,
		bool do_rfsh, bool refresh_v, u32 wait_time)
{
	int ret;
	u8 offset_reg_data;
	int cnt;
	uint64_t tstamp_ms;

	/* protect the access to the chip */
	mutex_lock(&chip_info->lock);

	if (do_rfsh) {
		ret = pac193x_send_rfsh(chip_info, refresh_v, wait_time);
		if (ret < 0) {
			pr_err("cannot send refresh towards PAC193x\n");
			goto reg_snapshot_err;
		}
	}
	tstamp_ms = ktime_get_boot_ns()/1000000;
	/* read the ctrl/status registers for this snapshot */
	ret = pac193x_i2c_read(chip_info->client, PAC193X_CTRL_STAT_REGS_ADDR,
			       (u8 *) chip_info->chip_reg_data.ctrl_regs,
			       PAC193X_CTRL_REG_SNAPSHOT_LEN);
	if (ret < 0) {
		pr_err("cannot read regs from 0x%02X\n",
			PAC193X_CTRL_STAT_REGS_ADDR);
		goto reg_snapshot_err;
	}

	/* read the data registers */
	ret = pac193x_i2c_read(chip_info->client, PAC193X_ACC_COUNT_REG,
			       (u8 *) chip_info->chip_reg_data.meas_regs,
			       PAC193X_MEAS_REG_SNAPSHOT_LEN);
	if (ret < 0) {
		pr_err("cannot read regs from 0x%02X\n",
			PAC193X_ACC_COUNT_REG);
		goto reg_snapshot_err;
	}

	chip_info->chip_reg_data.tstamp_ms = tstamp_ms;
	offset_reg_data = 0;
	chip_info->chip_reg_data.acc_count =
	mACC_COUNT(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
	/* move the register offset */
	offset_reg_data += PAC193X_ACC_REG_LEN;
	/* start with VPOWER_ACC */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		/* check if the channel is active(within the data read from
		 * the chip), skip all fields if disabled
		 */
		if ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* add the power_acc field */
		if (chip_info->chip_reg_data.bi_dir[cnt]) {
			/* bi-directional channel */
			chip_info->chip_reg_data.vpower_acc[cnt] =
	mVPOWER_ACCs(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		} else {
			/* uni-directional channel */
			chip_info->chip_reg_data.vpower_acc[cnt] =
	mVPOWER_ACCu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		}
		offset_reg_data += PAC193X_VPOWER_ACC_REG_LEN;

		chip_info->chip_reg_data.energy_sec_acc[cnt] +=
			(s64)(chip_info->chip_reg_data.vpower_acc[cnt]);
	}
	/* continue with VBUS */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
	/* check if the channel is active, skip all fields if disabled */
		if ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* read the VBUS channels */
		chip_info->chip_reg_data.vbus[cnt] =
	mVBUS_SENSEu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
			offset_reg_data += PAC193X_VBUS_SENSE_REG_LEN;
	}
	/* VSENSE */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
	/* check if the channel is active, skip all fields if disabled */
		if ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* read the VSENSE registers */
		if (chip_info->chip_reg_data.bi_dir[cnt]) {
			/* bi-directional channel */
			chip_info->chip_reg_data.vsense[cnt] =
	mVBUS_SENSEs(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		} else {
			/* uni-directional channel */
			chip_info->chip_reg_data.vsense[cnt] =
	mVBUS_SENSEu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		}
		offset_reg_data += PAC193X_VBUS_SENSE_REG_LEN;
	}
	/* VBUS_AVG */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
	/* check if the channel is active, skip all fields if disabled */
		if  ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* read the VBUS_AVG registers */
		chip_info->chip_reg_data.vbus_avg[cnt] =
	mVBUS_SENSEu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
			offset_reg_data += PAC193X_VBUS_SENSE_REG_LEN;
	}
	/* VSENSE_AVG */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
	/* check if the channel is active, skip all fields if disabled */
		if ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* read the VSENSE_AVG registers */
		if (chip_info->chip_reg_data.bi_dir[cnt]) {
			/* bi-directional channel */
			chip_info->chip_reg_data.vsense_avg[cnt] =
	mVBUS_SENSEs(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		} else {
			/* uni-directional channel */
			chip_info->chip_reg_data.vsense_avg[cnt] =
	mVBUS_SENSEu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		}
		offset_reg_data += PAC193X_VBUS_SENSE_REG_LEN;
	}
	/* VPOWER */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
	/* check if the channel is active, skip all fields if disabled */
		if ((
chip_info->chip_reg_data.ctrl_regs[PAC193X_CHANNEL_DIS_LAT_REG_OFF] << cnt) &
			0x80)
			continue;

		/* read the VPOWER fields */
		if (chip_info->chip_reg_data.bi_dir[cnt]) {
			/* bi-directional channel */
			chip_info->chip_reg_data.vpower[cnt] =
	mVPOWERs(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		} else {
			/* uni-directional channel */
			chip_info->chip_reg_data.vpower[cnt] =
	mVPOWERu(&chip_info->chip_reg_data.meas_regs[offset_reg_data]);
		}
		offset_reg_data += PAC193X_VPOWER_REG_LEN;

	}
reg_snapshot_err:
	mutex_unlock(&chip_info->lock);
	return ret;
}

static void pac193x_work_periodic_rfsh(struct work_struct *work)
{
	struct pac193x_chip_info *chip_info = to_pac193x_chip_info(work);
	int ret;
	/* do a REFRESH, then read */
	ret = pac193x_reg_snapshot(chip_info, true, false,
			PAC193x_MIN_UPDATE_WAIT_TIME);
}

void pac193x_read_reg_timeout(struct timer_list *t)
{
	int ret;
	struct pac193x_chip_info *chip_info = from_timer(chip_info, t, tmr_forced_update);

	ret = mod_timer(&chip_info->tmr_forced_update,
		jiffies + msecs_to_jiffies(PAC193X_MAX_RFSH_LIMIT));
	if (ret < 0)
		pr_err("forced read timer cannot be modified!\n");
	/* schedule the periodic reading from the chip */
	queue_work(chip_info->wq_chip, &chip_info->work_chip_rfsh);
}

static int pac193x_chip_identify(struct pac193x_chip_info *chip_info)
{
	int ret = 0;
	struct i2c_client *client = chip_info->client;
	u8 chip_rev_info[3];
	const char *reg_name;

	/* If regulator has not been defined in the device tree,
	 * print a debug warning, but assume that its always on,
	 * and proceed with the probe
	 * else get the regulator and enable it.
	 */

	if (of_property_read_string(client->dev.of_node,
				    "pac193x,regulator_avdd", &reg_name)) {
		pr_debug("failed to find regulator\n");
	} else {
		chip_info->avdd_reg = regulator_get(&client->dev, reg_name);
		if (IS_ERR(chip_info->avdd_reg)) {
			pr_err("failed to get regulator\n");
			if (chip_info->avdd_reg) {
				regulator_put(chip_info->avdd_reg);
				chip_info->avdd_reg = NULL;
			}
			return -EINVAL;
		}
		ret = regulator_enable(chip_info->avdd_reg);
		if (ret < 0) {
			pr_err("failed to enable regulator\n");
			return ret;
		}

		/* Spec defines time to first communcation as 14.25ms typical */
		msleep(20);
	}

	/* Try to identify the chip variant.
	 * Read the chip ID values.
	 */
	ret = pac193x_i2c_read(chip_info->client, PAC193X_PID_REG_ADDR,
				(u8 *) chip_rev_info, 3);
	if (ret < 0) {
		pr_err("cannot read revision\n");
		goto chip_identify_err;
	}
	if (chip_rev_info[0] !=
		pac193x_chip_config[chip_info->chip_variant].prod_id) {
		ret = -EINVAL;
		pr_err("chip's product ID doesn't match\n");
		goto chip_identify_err;
	}
	dev_info(&client->dev, "chip revision: 0x%02X\n", chip_rev_info[2]);
	chip_info->chip_revision = chip_rev_info[2];
chip_identify_err:
	return ret;
}

static int pac193x_setup_periodic_refresh(struct pac193x_chip_info *chip_info)
{
	int ret = 0;

	chip_info->wq_chip = create_workqueue("wq_pac193x");
	INIT_WORK(&chip_info->work_chip_rfsh, pac193x_work_periodic_rfsh);

	/* setup the latest moment for reading the regs before saturation */
	timer_setup(&chip_info->tmr_forced_update, pac193x_read_reg_timeout, 0);

	chip_info->tmr_forced_update.expires = jiffies +
			msecs_to_jiffies(PAC193X_MAX_RFSH_LIMIT);
	chip_info->forced_reads_triggered = 0;
	mod_timer(&chip_info->tmr_forced_update, chip_info->tmr_forced_update.expires);

	return ret;
}

static const char *pac193x_match_of_device(struct i2c_client *client,
					struct pac193x_chip_info *chip_info)
{
	struct device_node *node;
	unsigned int crt_ch;
	const char *ptr_name;

	ptr_name = pac193x_get_of_match_entry(client);

	if (of_property_read_u32(client->dev.of_node, "samp-rate",
				&chip_info->sample_rate_value)) {
		pr_err("cannot read sample rate value\n");
		return NULL;
	}
	if (pac193x_match_samp_rate(chip_info, chip_info->sample_rate_value)) {
		pr_err("the given sample rate value is not supported: %d\n",
			chip_info->sample_rate_value);
		return NULL;
	}
	crt_ch = 0;
	for_each_child_of_node(client->dev.of_node, node) {
		if (crt_ch >= chip_info->phys_channels)
			return NULL;
		/* check if the channel is enabled or not */
		chip_info->chip_reg_data.active_channels[crt_ch] =
			of_property_read_bool(node, "channel_enabled");

		/* read rail name */
		if (of_property_read_string(node,
					    "rail-name",
					    &chip_info->rail_name[crt_ch])) {
			pr_err("invalid rail-name value on %s\n",
				node->full_name);
		}

		if (of_property_read_string(node,
					    "subsys-name",
					    &chip_info->subsys_name[crt_ch])) {
			pr_err("invalid subsys-name value on %s\n",
				node->full_name);
		}

		if (!chip_info->chip_reg_data.active_channels[crt_ch]) {
		/* set the shunt value to 0 for the disabled channels */
			chip_info->shunts[crt_ch] = 0;
			crt_ch++;
			continue;
		}

		if (of_property_read_u32(node,
			"uohms-shunt-res", &chip_info->shunts[crt_ch])) {
			pr_err("invalid shunt-resistor value on %s\n",
				node->full_name);
			return NULL;
		}

		chip_info->chip_reg_data.bi_dir[crt_ch] =
				of_property_read_bool(node, "bi-dir");
		/* increment the channel index */
		crt_ch++;
	}
	return ptr_name;
}

static int pac193x_chip_configure(struct pac193x_chip_info *chip_info)
{
	int cnt, ret = 0;
	u8 regs[PAC193X_CTRL_STATUS_INFO_LEN];
	u32 wait_time;

	/* count how many channels are enabled and store
	 * this information within the driver data
	 */
	cnt = 0;
	chip_info->chip_reg_data.num_enabled_channels = 0;
	while (cnt < chip_info->phys_channels) {
		if (chip_info->chip_reg_data.active_channels[cnt])
			chip_info->chip_reg_data.num_enabled_channels++;
		cnt++;
	}
	/* read whatever information was gathered before the driver was loaded
	 * establish which channels are enabled/disabled and then establish the
	 * information retrieval mode (using SKIP or no).
	 * Read the chip ID values
	 */
	ret = pac193x_i2c_read(chip_info->client, PAC193X_CTRL_STAT_REGS_ADDR,
				(u8 *)regs, PAC193X_CTRL_STATUS_INFO_LEN);
	if (ret < 0) {
		pr_err("cannot read regs from 0x%02X\n",
			PAC193X_CTRL_STAT_REGS_ADDR);
		goto chip_configure_err;
	}
	/* write the CHANNEL_DIS and the NEG_PWR registers */
	regs[PAC193X_CHANNEL_DIS_REG_OFF] =
		CHANNEL_DIS_REG(chip_info->chip_reg_data.active_channels[0],
				chip_info->chip_reg_data.active_channels[1],
				chip_info->chip_reg_data.active_channels[2],
				chip_info->chip_reg_data.active_channels[3],
				0, 0, 1);

	regs[PAC193X_NEG_PWR_REG_OFF] =
		NEG_PWR_REG(chip_info->chip_reg_data.bi_dir[0],
			chip_info->chip_reg_data.bi_dir[1],
			chip_info->chip_reg_data.bi_dir[2],
			chip_info->chip_reg_data.bi_dir[3],
			0, 0, 0, 0);
	/* the current can be measured uni or bi-dir, but voltages are set only
	 * for uni-directional operation
	 * no SLOW triggered REFRESH, clear POR
	 */
	regs[PAC193X_SLOW_REG_OFF] = 0;
	/* write the updated registers back */
	ret = pac193x_i2c_write(chip_info->client, PAC193X_CTRL_STAT_REGS_ADDR,
				3, (u8 *)regs);
	if (ret < 0) {
		pr_err("cannot write regs from 0x%02X\n",
			PAC193X_CHANNEL_DIS_REG_OFF);
		goto chip_configure_err;
	}
	/* enable the ALERT pin functionality */
	ret = pac193x_i2c_write_byte(chip_info->client, PAC193X_CTRL_REG,
		CTRL_REG(chip_info->chip_reg_data.crt_samp_speed_bitfield,
		0, 0, 1, 0, 0));
	if (ret < 0) {
		pr_err("cannot write PAC193x ctrl reg at 0x%02X\n",
			PAC193X_CTRL_REG);
		goto chip_configure_err;
	}
	/* send a REFRESH to the chip, so the new settings take place
	 * as well as resetting the accumulators
	 */
	ret = pac193x_i2c_send_byte(chip_info->client, PAC193X_REFRESH_REG);
	if (ret < 0) {
		pr_err("cannot send byte to 0x%02X reg\n",
			PAC193X_REFRESH_REG);
		return ret;
	}

	/* get the current(in the chip) sampling speed and compute the
	 * required timeout based on its value
	 * the timeout is 1/sampling_speed
	 */
	wait_time = (1024 /
		samp_rate_map_tbl[(regs[PAC193X_CTRL_ACT_REG_OFF]>>6)]) * 1000;
	/* wait the maximum amount of time to be on the safe side - the
	 * maximum wait time is for 8sps
	 */
	usleep_range(wait_time, wait_time + 100);
	/* setup the refresh timeout */
	ret = pac193x_setup_periodic_refresh(chip_info);
chip_configure_err:
	return ret;
}

static int pac193x_retrieve_data(struct pac193x_chip_info *chip_info,
					u32 wait_time)
{
	int ret = 0;
	/* check if the minimal elapsed time has passed and if so,
	 * re-read the chip, otherwise the cached info is just fine
	 */
	if (time_after(jiffies, chip_info->chip_reg_data.jiffies_tstamp +
		msecs_to_jiffies(PAC193X_MIN_POLLING_TIME))) {
		/* we need to re-read the chip values
		 * call the pac193x_reg_snapshot
		 */
		ret = pac193x_reg_snapshot(chip_info, true, false, wait_time);
		/* re-schedule the work for the read registers timeout
		 * (to prevent chip regs saturation)
		 */
		ret = mod_timer(&chip_info->tmr_forced_update,
			chip_info->chip_reg_data.jiffies_tstamp +
			msecs_to_jiffies(PAC193X_MAX_RFSH_LIMIT));
		if (ret < 0)
			pr_err("read timer can't be modified!\n");
	}
	return ret;
}

static int pac193x_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	struct pac193x_chip_info *chip_info;
	struct iio_dev *indio_dev;
	const char *name = NULL;
	int cnt, ret = 0;
	int dev_id = 0;

	/* allocate the memory for our private structure
	 * related to the chip info structure
	 */
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*chip_info));
	if (!indio_dev)
		return -ENOMEM;
	/* point our chip info structure towards
	 * the address freshly allocated
	 */
	chip_info = iio_priv(indio_dev);
	/* make the link between the I2C slave client driver and the IIO */
	i2c_set_clientdata(client, indio_dev);
	chip_info->client = client;

	/* get the name and the dev_id */
	name = id->name;
	dev_id = id->driver_data;
	/* store the type of chip */
	chip_info->chip_variant = dev_id;
	/* get the maximum number of channels for the given chip id */
	chip_info->phys_channels = pac193x_chip_config[dev_id].phys_channels;
	/* clear the chip-related structure */
	memset(&chip_info->chip_reg_data, 0, sizeof(chip_info->chip_reg_data));
	/* load default settings - all channels enabled,
	 * uni directional flow, default shunt values
	 */
	for (cnt = 0; cnt < chip_info->phys_channels; cnt++) {
		chip_info->chip_reg_data.active_channels[cnt] = true;
		chip_info->chip_reg_data.bi_dir[cnt] = false;
		chip_info->shunts[cnt] = SHUNT_UOHMS_DEFAULT;
		chip_info->rail_name[cnt] = "";
		chip_info->subsys_name[cnt] = "";
	}
	chip_info->chip_reg_data.crt_samp_speed_bitfield = PAC193x_SAMP_1024SPS;

	/* identify the chip we have to deal with */
	ret = pac193x_chip_identify(chip_info);
	if (ret < 0)
		return -EINVAL;
	/* check if we find the device within DT
	 * if no settings are available, use the defaults
	 */
	if ((!client->dev.of_node) ||
		(!of_get_next_child(client->dev.of_node, NULL))) {
		return -EINVAL;
	}
	/* we have DT */
	name = pac193x_match_of_device(client, chip_info);
	if (!name) {
		pr_err("DT parameter parsing returned an error\n");
		return -EINVAL;
	}

	/* initialize the chip access mutex */
	mutex_init(&chip_info->lock);
	/* do now any chip specific initialization (e.g. read/write
	 * some registers), enable/disable certain channels, change the sampling
	 * rate to the requested value
	 */
	ret = pac193x_chip_configure(chip_info);
	if (ret < 0)
		goto free_chan_attr_mem;
	/* configure the IIO related fields and register this device with IIO */
	indio_dev->info = &pac193x_info;
	indio_dev->name = name;
	indio_dev->dev.parent = &client->dev;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->driver_module = THIS_MODULE;
	/* read whatever it has been accumulated in the chip so far
	 * and reset the accumulators
	 */
	ret = pac193x_reg_snapshot(chip_info, true, false,
					PAC193x_MIN_UPDATE_WAIT_TIME);
	/* register with IIO */
	ret = devm_iio_device_register(&client->dev, indio_dev);
	if (ret < 0) {
free_chan_attr_mem:
		pac193x_remove(client);
	}
	return ret;
}

static int pac193x_remove(struct i2c_client *client)
{
	int ret = 0;
	struct iio_dev *indio_dev = dev_get_drvdata(&client->dev);
	struct pac193x_chip_info *chip_info = iio_priv(indio_dev);
	/* free the channel attributes memory */
	kfree(indio_dev->channels);
	ret = try_to_del_timer_sync(&chip_info->tmr_forced_update);
	if (ret < 0) {
		pr_err("cannot delete the forced PAC193x readout timer\n");
		return ret;
	}
	if (chip_info->wq_chip != NULL) {
		cancel_work_sync(&chip_info->work_chip_rfsh);
		flush_workqueue(chip_info->wq_chip);
		destroy_workqueue(chip_info->wq_chip);
	}
	return ret;
}

static const struct i2c_device_id pac193x_id[] = {
	{"pac1934", pac1934},
	{"pac1932", pac1932},
	{"pac1931", pac1931},
	{}
};
MODULE_DEVICE_TABLE(i2c, pac193x_id);

static const struct of_device_id pac193x_of_match[] = {
	{ .compatible = "microchip,pac1934",
		.data = (void *)&pac193x_chip_config[pac1934]},
	{ .compatible = "microchip,pac1932",
		.data = (void *)&pac193x_chip_config[pac1932]},
	{ .compatible = "microchip,pac1931",
		.data = (void *)&pac193x_chip_config[pac1931]},
	{ },
};
MODULE_DEVICE_TABLE(of, pac193x_of_match);

static const char *pac193x_get_of_match_entry(struct i2c_client *client)
{
	const struct of_device_id *match;

	match = of_match_node(pac193x_of_match, client->dev.of_node);
	return match->compatible;
}

static struct i2c_driver pac193x_driver = {
	.driver	 = {
			.name = "pac193x",
			.of_match_table = pac193x_of_match,
		    },
	.probe	 = pac193x_probe,
	.remove		= pac193x_remove,
	.id_table = pac193x_id,
};

module_i2c_driver(pac193x_driver);

MODULE_AUTHOR("Bogdan Bolocan");
MODULE_DESCRIPTION("PAC193x");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.0.1");
