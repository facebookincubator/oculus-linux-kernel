// SPDX-License-Identifier: GPL-2.0-only
/*
 * w1_ds28e18.c - w1 family 56 (DS28E18) driver
 *
 * Copyright (c) 2024 Meta Platforms, Inc.
 * Based on w1_ds28e17.c by Jan Kandziora <jjj@gmx.de>
 */

#include <linux/crc16.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/w1.h>

#define CRC16_INIT 0
#define W1_FAMILY_DS28E18 0x56

/* Module setup. */
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("w1 family 56 driver for DS28E18, 1-wire to I2C bridge with command sequencer");
MODULE_ALIAS("w1-family-" __stringify(W1_FAMILY_DS28E18));

static int i2c_speed = 400;
module_param_named(speed, i2c_speed, int, 0600);
MODULE_PARM_DESC(speed, "Default I2C speed in KHz to be set (100, 400, 1000, 2300)");

/* DS28E18 device function command codes */
#define W1_F56_COMMAND_START			0x66
#define W1_F56_WRITE_SEQUENCER			0x11
#define W1_F56_READ_SEQUENCER			0x22
#define W1_F56_RUN_SEQUENCER			0x33
#define W1_F56_WRITE_CONFIGURATION		0x55
#define W1_F56_READ_CONFIGURATION		0x6A
#define W1_F56_WRITE_GPIO_CONFIGURATION		0x83
#define W1_F56_READ_GPIO_CONFIGURATION		0x7C
#define W1_F56_DEVICE_STATUS			0x7A

/* DS28E18 sequencer command codes */
#define W1_F56_I2C_START			0x02
#define W1_F56_I2C_STOP				0x03
#define W1_F56_I2C_WRITE_DATA			0xE3
#define W1_F56_I2C_READ_DATA			0xD4
#define W1_F56_I2C_READ_DATA_NACK_END		0xD3
#define W1_F56_SPI_WRITE_READ_BYTE		0xC0
#define W1_F56_SPI_WRITE_READ_BIT		0xB0
#define W1_F56_SS_HIGH				0x01
#define W1_F56_SS_LOW				0x80
#define W1_F56_DELAY				0xDD
#define W1_F56_SENS_VDD_ON			0xCC
#define W1_F56_SENS_VDD_OFF			0xBB
#define W1_F56_GPIO_BUF_WRITE			0xD1
#define W1_F56_GPIO_BUF_READ			0x1D
#define W1_F56_GPIO_CTRL_WRITE			0xE2
#define W1_F56_GPIO_CTRL_READ			0x2E

/* DS28E18 release byte */
#define W1_F56_RELEASE_BYTE			0xAA

/* DS28E18 result codes */
#define W1_F56_RESULT_SUCCESS			0xAA
#define W1_F56_RESULT_INVALID_PARAM		0x77
#define W1_F56_RESULT_POR_ERROR			0x44
#define W1_F56_RESULT_EXEC_ERROR		0x55
#define W1_F56_RESULT_I2C_NACK			0x88

/* Configuration register bits */
#define W1_F56_CONFIG_SPD_MASK			(BIT(0)|BIT(1))
#define W1_F56_CONFIG_SPD_100KHZ_VAL		0x00
#define W1_F56_CONFIG_SPD_400KHZ_VAL		0x01
#define W1_F56_CONFIG_SPD_1MHZ_VAL		0x02
#define W1_F56_CONFIG_SPD_2_3MHZ_VAL		0x03
#define W1_F56_CONFIG_INACK_MASK		BIT(2)
#define W1_F56_CONFIG_INACK_VAL			BIT(2)
#define W1_F56_CONFIG_PROT_MASK			BIT(3)
#define W1_F56_CONFIG_PROT_I2C_VAL		0
#define W1_F56_CONFIG_PROT_SPI_VAL		BIT(3)
#define W1_F56_CONFIG_SPI_MODE_0		0x00
#define W1_F56_CONFIG_SPI_MODE_3		0x30

/* Maximum data limits */
#define W1_F56_SEQUENCER_SIZE			512
#define W1_F56_WRITE_DATA_LIMIT			128
#define W1_F56_READ_DATA_LIMIT			128
#define W1_F56_MAX_SEQUENCE_LEN			256

/* Operation timing (microseconds) */
#define W1_F56_OPERATION_TIME_US		1000

/* Slave specific data */
struct w1_f56_data {
	u8 config;
	struct i2c_adapter i2c_adapter;
	struct spi_controller *spi_controller;
};

/* Calculate sequencer execution time based on speed and sequence length */
static int w1_f56_calculate_sequencer_time(struct w1_f56_data *data, u16 seq_len)
{
	int time_us;

	/* Add sequencer communication time based on I2C speed */
	switch (data->config & W1_F56_CONFIG_SPD_MASK) {
	case W1_F56_CONFIG_SPD_2_3MHZ_VAL:
		time_us = (seq_len * 17);  /* ~17us per byte at 2.3MHz */
		break;
	case W1_F56_CONFIG_SPD_1MHZ_VAL:
		time_us = (seq_len * 25);  /* ~25us per byte at 1MHz */
		break;
	case W1_F56_CONFIG_SPD_400KHZ_VAL:
		time_us = (seq_len * 50);  /* ~42us per byte at 400kHz */
		break;
	default:
	case W1_F56_CONFIG_SPD_100KHZ_VAL:
		time_us = (seq_len * 123); /* ~123us per byte at 100kHz */
		break;
	}

	return max(W1_F56_OPERATION_TIME_US, time_us);
}

static int w1_f56_command_start(struct w1_slave *sl, u8 command,
				const u8 *params, int param_len,
				u8 *result_buf, int result_buf_size,
				int *result_len)
{
	struct w1_f56_data *data = sl->family_data;
	u16 crc_calc, crc_recv;
	u8 length_byte;
	u8 release_byte = W1_F56_RELEASE_BYTE;
	u8 command_start = W1_F56_COMMAND_START;
	int wait_time = W1_F56_OPERATION_TIME_US;
	int i;

	/* Calculate length: command + parameters */
	length_byte = 1 + param_len;

	if (w1_reset_select_slave(sl)) {
		dev_err(&sl->dev, "Reset/select faled\n");
		return -EIO;
	}

	/* Send command start sequence */
	w1_write_8(sl->master, command_start);
	w1_write_8(sl->master, length_byte);
	w1_write_8(sl->master, command);

	/* Send parameters if any */
	if (param_len > 0 && params)
		w1_write_block(sl->master, params, param_len);

	/* Calculate expected CRC16 */
	crc_calc = crc16(CRC16_INIT, &command_start, 1);
	crc_calc = crc16(crc_calc, &length_byte, 1);
	crc_calc = crc16(crc_calc, &command, 1);
	if (param_len > 0 && params)
		crc_calc = crc16(crc_calc, params, param_len);

	/* RECEIVE CRC16 from device (inverted) */
	crc_recv = w1_read_8(sl->master);		/* Low byte */
	crc_recv |= (w1_read_8(sl->master) << 8);	/* High byte */
	crc_recv = ~crc_recv;  /* Un-invert */

	/* Validate CRC16 */
	if (crc_recv != crc_calc) {
		dev_err(&sl->dev, "Command CRC mismatch: expected 0x%04x, got 0x%04x\n",
				crc_calc, crc_recv);
		return -EIO;
	}

	/* Send release byte */
	w1_write_8(sl->master, release_byte);

	/* Calculate minimum wait time */
	if (command == W1_F56_RUN_SEQUENCER && param_len >= 3) {
		u16 seq_len = params[1] >> 1;
		seq_len |= (params[2] & 0x03) << 7;
		if (seq_len == 0)
			seq_len = 512;
		wait_time = w1_f56_calculate_sequencer_time(data, seq_len);
	}

	/* Wait for operation to complete */
	usleep_range(wait_time, wait_time + 1000);

	/* Read dummy byte */
	w1_read_8(sl->master);

	/* Read result length */
	length_byte = w1_read_8(sl->master);

	if (length_byte == 0) {
		*result_len = 0;
		return -EOPNOTSUPP;
	}

	/* BOUNDS CHECK */
	if (length_byte > result_buf_size) {
		dev_err(&sl->dev, "Result length %d for command 0x%02X exceeds buffer size %d\n",
			length_byte, command, result_buf_size);
		*result_len = 0;
		return -EMSGSIZE;
	}

	/* Read result data */
	*result_len = length_byte;
	for (i = 0; i < length_byte; i++)
		result_buf[i] = w1_read_8(sl->master);

	/* RECEIVE result CRC16 (inverted) and validate */
	crc_recv = w1_read_8(sl->master);		/* Low byte */
	crc_recv |= (w1_read_8(sl->master) << 8);	/* High byte */
	crc_recv = ~crc_recv;  /* Un-invert */

	/* Calculate expected result CRC16 */
	crc_calc = crc16(CRC16_INIT, &length_byte, 1);
	crc_calc = crc16(crc_calc, result_buf, *result_len);

	/* Validate result CRC16 */
	if (crc_recv != crc_calc) {
		dev_warn(&sl->dev, "Result CRC mismatch: expected 0x%04x, got 0x%04x\n",
				crc_calc, crc_recv);
		/* Don't fail here, just warn */
	}

	return 0;
}

static int w1_f56_ensure_device_ready(struct w1_slave *sl)
{
	u8 result_buf[8];
	int result_len;
	int ret;
	/* Read device status */
	ret = w1_f56_command_start(sl, W1_F56_DEVICE_STATUS,
		NULL, 0, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;
	if (result_len >= 2) {
		u8 status = result_buf[1];
		dev_dbg(&sl->dev, "Device status: 0x%02x\n", status);

		if (status & 0x02) {  /* POR bit set */
			dev_warn(&sl->dev, "Device POR bit set - SRAM contents invalid\n");
			/* Clear POR bit by reading status again */
			ret = w1_f56_command_start(sl, W1_F56_DEVICE_STATUS,
				NULL, 0, result_buf, sizeof(result_buf), &result_len);
			return -EAGAIN;  /* Caller should reinitialize */
		}
	}
	return 0;
}

/* Write configuration register */
static int w1_f56_write_config(struct w1_slave *sl, u8 config)
{
	u8 result_buf[2];
	int result_len;
	int ret;

	ret = w1_f56_command_start(sl, W1_F56_WRITE_CONFIGURATION,
				   &config, 1, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;

	if (result_len < 1 || result_buf[0] != W1_F56_RESULT_SUCCESS)
		return -EIO;

	return 0;
}

/* Read configuration register */
static int w1_f56_read_config(struct w1_slave *sl, u8 *config)
{
	u8 result_buf[3];
	int result_len;
	int ret;

	ret = w1_f56_command_start(sl, W1_F56_READ_CONFIGURATION,
				   NULL, 0, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;

	if (result_len < 2 || result_buf[0] != W1_F56_RESULT_SUCCESS)
		return -EIO;

	*config = result_buf[1];

	return 0;
}

/* Write to sequencer SRAM */
static int w1_f56_write_sequencer(struct w1_slave *sl, u16 addr,
				  const u8 *data, int len)
{
	u8 params[2 + W1_F56_WRITE_DATA_LIMIT];
	u8 result_buf[2];
	int result_len;
	int ret;

	if (len > W1_F56_WRITE_DATA_LIMIT)
		return -EINVAL;

	/* Address low, address high, data */
	params[0] = addr & 0xFF;
	params[1] = (addr >> 8) & 0x01;
	memcpy(&params[2], data, len);

	ret = w1_f56_command_start(sl, W1_F56_WRITE_SEQUENCER,
				   params, 2 + len, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;

	if (result_len < 1 || result_buf[0] != W1_F56_RESULT_SUCCESS)
		return -EIO;

	return 0;
}

/* Read from sequencer SRAM */
static int w1_f56_read_sequencer(struct w1_slave *sl, u16 addr,
				 u8 *data, int len)
{
	u8 params[2];
	u8 result_buf[1 + W1_F56_READ_DATA_LIMIT];
	int result_len;
	int ret;

	if (len > W1_F56_READ_DATA_LIMIT)
		return -EINVAL;

	/* Address low, (length << 1) | address high */
	params[0] = addr & 0xFF;
	params[1] = ((len & 0x7F) << 1) | ((addr >> 8) & 0x01);

	ret = w1_f56_command_start(sl, W1_F56_READ_SEQUENCER,
				   params, 2, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;

	if (result_len < 1 || result_buf[0] != W1_F56_RESULT_SUCCESS)
		return -EIO;

	memcpy(data, &result_buf[1], min(len, result_len - 1));

	return min(len, result_len - 1);
}

/* Run sequencer */
static int w1_f56_run_sequencer(struct w1_slave *sl, u16 addr, u16 len)
{
	u8 params[3];
	u8 result_buf[256];  /* Increased buffer size */
	int result_len;
	int ret;
	/* Address low, (length low << 1) | address high, length high */
	params[0] = addr & 0xFF;
	params[1] = ((len & 0x7F) << 1) | ((addr >> 8) & 0x01);
	params[2] = (len >> 7) & 0x03;

	ret = w1_f56_command_start(sl, W1_F56_RUN_SEQUENCER,
				   params, 3, result_buf, sizeof(result_buf), &result_len);
	if (ret < 0)
		return ret;

	if (result_len < 1)
		return -EIO;

	switch (result_buf[0]) {
	case W1_F56_RESULT_SUCCESS:
		return 0;
	case W1_F56_RESULT_I2C_NACK:
		if (result_len >= 3) {
			dev_warn(&sl->dev, "I2C NACK at offset %d\n",
				 result_buf[1] | (result_buf[2] << 8));
		}
		return -ENXIO;
	case W1_F56_RESULT_POR_ERROR:
		dev_warn(&sl->dev, "Power-on reset occurred\n");
		return -EAGAIN;
	case W1_F56_RESULT_EXEC_ERROR:
		dev_warn(&sl->dev, "Sequencer execution error\n");
		return -EIO;
	default:
		dev_warn(&sl->dev, "Unknown result code: 0x%02x\n", result_buf[0]);
		return -EIO;
	}
}

/* Build I2C write sequence in sequencer SRAM */
static int w1_f56_build_i2c_write_sequence(struct w1_slave *sl, u16 addr,
					   u8 i2c_addr, const u8 *data, int len)
{
	u8 seq_buf[W1_F56_MAX_SEQUENCE_LEN];
	int seq_len = 0;

	if (len > W1_F56_MAX_SEQUENCE_LEN - 6)
		return -EINVAL;

	/* I2C Start */
	seq_buf[seq_len++] = W1_F56_I2C_START;

	/* I2C Write Data */
	seq_buf[seq_len++] = W1_F56_I2C_WRITE_DATA;
	seq_buf[seq_len++] = 1 + len;  /* Length: address + data */
	seq_buf[seq_len++] = i2c_addr << 1;  /* I2C address with write bit */
	memcpy(&seq_buf[seq_len], data, len);
	seq_len += len;

	/* I2C Stop */
	seq_buf[seq_len++] = W1_F56_I2C_STOP;

	return w1_f56_write_sequencer(sl, addr, seq_buf, seq_len);
}

/* Build I2C read sequence in sequencer SRAM */
static int w1_f56_build_i2c_read_sequence(struct w1_slave *sl, u16 addr,
					  u8 i2c_addr, int len)
{
	u8 seq_buf[W1_F56_MAX_SEQUENCE_LEN];
	int seq_len = 0;
	int i;

	if (len > W1_F56_MAX_SEQUENCE_LEN - 6)
		return -EINVAL;

	/* I2C Start */
	seq_buf[seq_len++] = W1_F56_I2C_START;

	/* I2C Write Data (address only) */
	seq_buf[seq_len++] = W1_F56_I2C_WRITE_DATA;
	seq_buf[seq_len++] = 1;  /* Length: address only */
	seq_buf[seq_len++] = (i2c_addr << 1) | 0x01;  /* I2C address with read bit */

	/* I2C Read Data with NACK end */
	seq_buf[seq_len++] = W1_F56_I2C_READ_DATA_NACK_END;
	seq_buf[seq_len++] = len;  /* Number of bytes to read */
	for (i = 0; i < len; i++)
		seq_buf[seq_len++] = 0xFF;  /* Placeholder for read data */

	/* I2C Stop */
	seq_buf[seq_len++] = W1_F56_I2C_STOP;

	return w1_f56_write_sequencer(sl, addr, seq_buf, seq_len);
}

/* I2C master transfer implementation */
static int w1_f56_i2c_master_transfer(struct i2c_adapter *adapter,
				      struct i2c_msg *msgs, int num)
{
	struct w1_slave *sl = (struct w1_slave *)adapter->algo_data;
	int i, ret;
	u16 seq_addr = 0;

	/* Start onewire transaction */
	mutex_lock(&sl->master->bus_mutex);

	/* Select DS28E18 */
	if (w1_reset_select_slave(sl)) {
		dev_err(&sl->dev, "Reset/select faled\n");
		ret = -EIO;
		goto error;
	}

	ret = w1_f56_ensure_device_ready(sl);
	if (ret && ret == -EAGAIN)
		ret = w1_f56_ensure_device_ready(sl);
	if (ret) {
		dev_err(&sl->dev, "Device not ready. Power-on-reset is set.\n");
		ret = -EIO;
		goto error;
	}

	/* Process each message */
	for (i = 0; i < num; i++) {
		if (msgs[i].flags & I2C_M_RD) {
			/* I2C Read Operation */
			ret = w1_f56_build_i2c_read_sequence(sl, seq_addr,
							     msgs[i].addr,
							     msgs[i].len);
			if (ret < 0)
				goto error;

			ret = w1_f56_run_sequencer(sl, seq_addr, 7 + msgs[i].len);
			if (ret < 0)
				goto error;

			ret = w1_f56_read_sequencer(sl, seq_addr + 6,
				msgs[i].buf, msgs[i].len);
			if (ret < 0)
				goto error;
		} else {
			/* I2C Write Operation */
			ret = w1_f56_build_i2c_write_sequence(sl, seq_addr,
							      msgs[i].addr,
							      msgs[i].buf,
							      msgs[i].len);
			if (ret < 0)
				goto error;

			ret = w1_f56_run_sequencer(sl, seq_addr, 4 + msgs[i].len);
			if (ret < 0)
				goto error;
		}

		seq_addr += 10 + msgs[i].len;  /* Move to next sequence position */
	}

	ret = num;

error:
	mutex_unlock(&sl->master->bus_mutex);
	return ret;
}

/* I2C functionality */
static u32 w1_f56_i2c_functionality(struct i2c_adapter *adapter)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

/* I2C algorithm */
static const struct i2c_algorithm w1_f56_i2c_algorithm = {
	.master_xfer = w1_f56_i2c_master_transfer,
	.functionality = w1_f56_i2c_functionality,
};

/* Sysfs attribute: speed */
static ssize_t speed_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(dev);
	u8 config;
	int speed, ret;

	mutex_lock(&sl->master->bus_mutex);
	ret = w1_f56_read_config(sl, &config);
	mutex_unlock(&sl->master->bus_mutex);
	if (ret < 0)
		return -EIO;

	switch (config & W1_F56_CONFIG_SPD_MASK) {
	case W1_F56_CONFIG_SPD_100KHZ_VAL:
		speed = 100;
		break;
	case W1_F56_CONFIG_SPD_400KHZ_VAL:
		speed = 400;
		break;
	case W1_F56_CONFIG_SPD_1MHZ_VAL:
		speed = 1000;
		break;
	case W1_F56_CONFIG_SPD_2_3MHZ_VAL:
		speed = 2300;
		break;
	default:
		speed = 0;
		break;
	}

	return sysfs_emit(buf, "%d\n", speed);
}

static ssize_t speed_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct w1_slave *sl = dev_to_w1_slave(dev);
	struct w1_f56_data *data = sl->family_data;
	int speed, ret;
	u8 config;

	ret = kstrtoint(buf, 10, &speed);
	if (ret < 0)
		return ret;

	mutex_lock(&sl->master->bus_mutex);
	ret = w1_f56_read_config(sl, &config);
	if (ret < 0)
		goto out;

	config &= ~W1_F56_CONFIG_SPD_MASK;
	switch (speed) {
	case 100:
		config |= W1_F56_CONFIG_SPD_100KHZ_VAL;
		break;
	case 400:
		config |= W1_F56_CONFIG_SPD_400KHZ_VAL;
		break;
	case 1000:
		config |= W1_F56_CONFIG_SPD_1MHZ_VAL;
		break;
	case 2300:
		config |= W1_F56_CONFIG_SPD_2_3MHZ_VAL;
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	ret = w1_f56_write_config(sl, config);
	if (ret < 0)
		goto out;

	data->config = config;
out:
	mutex_unlock(&sl->master->bus_mutex);
	if (ret < 0)
		return ret;

	return count;
}

static DEVICE_ATTR_RW(speed);

/* Sysfs attribute: protocol */
static ssize_t protocol_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct w1_slave *sl = dev_to_w1_slave(dev);
	u8 config;
	int ret;

	mutex_lock(&sl->master->bus_mutex);
	ret = w1_f56_read_config(sl, &config);
	mutex_unlock(&sl->master->bus_mutex);
	if (ret < 0)
		return ret;

	return sysfs_emit(buf, "%s\n",
			(config & W1_F56_CONFIG_PROT_MASK) == W1_F56_CONFIG_PROT_SPI_VAL ? "spi" : "i2c");
}

static DEVICE_ATTR_RO(protocol);

static struct attribute *w1_f56_attrs[] = {
	&dev_attr_speed.attr,
	&dev_attr_protocol.attr,
	NULL,
};

static const struct attribute_group w1_f56_group = {
	.attrs = w1_f56_attrs,
};

static const struct attribute_group *w1_f56_groups[] = {
	&w1_f56_group,
	NULL,
};

static int w1_f56_add_slave(struct w1_slave *sl)
{
	struct w1_f56_data *data;
	u8 config;
	u8 spd;
	int ret;

	data = devm_kzalloc(&sl->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	sl->family_data = data;

	mutex_lock(&sl->master->bus_mutex);

	ret = w1_f56_read_config(sl, &config);
	if (ret < 0) {
		dev_err(&sl->dev, "Unable to read config\n");
		mutex_unlock(&sl->master->bus_mutex);
		return -EIO;
	}

	switch (i2c_speed) {
	case 2300:
		spd = W1_F56_CONFIG_SPD_2_3MHZ_VAL;
		break;
	case 1000:
		spd = W1_F56_CONFIG_SPD_1MHZ_VAL;
		break;
	case 400:
		spd = W1_F56_CONFIG_SPD_400KHZ_VAL;
		break;
	case 100:
		spd = W1_F56_CONFIG_SPD_100KHZ_VAL;
		break;
	default:
		dev_err(&sl->dev, "Invalid I2C speed %dKHz\n", i2c_speed);
		mutex_unlock(&sl->master->bus_mutex);
		return -EINVAL;
	}

	if ((config & W1_F56_CONFIG_SPD_MASK) != spd) {
		config &= ~W1_F56_CONFIG_SPD_MASK;
		config |= spd;

		ret = w1_f56_write_config(sl, config);
		if (ret < 0) {
			mutex_unlock(&sl->master->bus_mutex);
			dev_err(&sl->dev, "Failed to configure DS28E18\n");
			return ret;
		}
	}

	/* Update our cached config */
	data->config = config;

	mutex_unlock(&sl->master->bus_mutex);

	/* Setup I2C adapter. SPI support not implemented. */
	data->i2c_adapter.owner = THIS_MODULE;
	data->i2c_adapter.algo = &w1_f56_i2c_algorithm;
	data->i2c_adapter.algo_data = sl;
	data->i2c_adapter.dev.parent = &sl->dev;

	snprintf(data->i2c_adapter.name, sizeof(data->i2c_adapter.name),
		 "DS28E18 1-Wire I2C bridge on %s", sl->name);

	ret = i2c_add_adapter(&data->i2c_adapter);
	if (ret < 0) {
		dev_err(&sl->dev, "Failed to add I2C adapter\n");
		return ret;
	}

	dev_info(&sl->dev, "DS28E18 I2C bridge registered\n");

	return 0;
}

/* Device remove function */
static void w1_f56_remove_slave(struct w1_slave *sl)
{
	struct w1_f56_data *data = sl->family_data;

	if (data) {
		i2c_del_adapter(&data->i2c_adapter);
		dev_info(&sl->dev, "DS28E18 I2C bridge unregistered\n");
	}
}

/* W1 family operations */
static const struct w1_family_ops w1_f56_fops = {
	.add_slave = w1_f56_add_slave,
	.remove_slave = w1_f56_remove_slave,
	.groups = w1_f56_groups,
};

/* W1 family structure */
static struct w1_family w1_family_56 = {
	.fid = W1_FAMILY_DS28E18,
	.fops = &w1_f56_fops,
};

module_w1_family(w1_family_56);
