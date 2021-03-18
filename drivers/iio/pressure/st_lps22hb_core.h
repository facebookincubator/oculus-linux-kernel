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

#ifndef __LPS22HB_H
#define __LPS22HB_H

#include <linux/types.h>
#include <linux/iio/iio.h>
#include <linux/iio/trigger.h>

#define LPS22HB_PRESS_LSB_PER_HPA		4096UL
#define LPS22HB_PRESS_KPASCAL_NANO_SCALE	(100000000UL / \
						 LPS22HB_PRESS_LSB_PER_HPA)

#define LPS22HB_TEMP_LSB_PER_CELSIUS		100UL
#define LPS22HB_TEMP_CELSIUS_NANO_SCALE		(1000000000UL / \
						 LPS22HB_TEMP_LSB_PER_CELSIUS)


#define LPS22HB_WHO_AM_I_ADDR			0x0f
#define LPS22HB_WHO_AM_I_DEF			0xb1
#define LPS22HB_INTERRUPT_CFG_ADDR		0x0b
#define LPS22HB_CTRL1_ADDR			0x10
#define LPS22HB_CTRL2_ADDR			0x11
#define LPS22HB_CTRL3_ADDR			0x12

#define LPS22HB_FIFO_CTRL_ADDR			0x14

#define LPS22HB_PRESS_OUT_XL_ADDR		0x28
#define LPS22HB_TEMP_OUT_L_ADDR			0x2b

#define LPS22HB_FIFO_THS_ADDR			LPS22HB_FIFO_CTRL_ADDR
#define LPS22HB_FIFO_THS_MASK			0x1f

#define LPS22HB_ODR_ADDR			LPS22HB_CTRL1_ADDR
#define LPS22HB_ODR_MASK			0x70
#define LPS22HB_ODR_POWER_OFF_VAL		0x00
#define LPS22HB_ODR_1HZ_VAL			0x01
#define LPS22HB_ODR_10HZ_VAL			0x02
#define LPS22HB_ODR_25HZ_VAL			0x03
#define LPS22HB_ODR_50HZ_VAL			0x04
#define LPS22HB_ODR_75HZ_VAL			0x05
#define LPS22HB_ODR_LIST_NUM			6

#define LPS22HB_PRESS_FS_AVL_GAIN		LPS22HB_PRESS_KPASCAL_NANO_SCALE
#define LPS22HB_TEMP_FS_AVL_GAIN		LPS22HB_TEMP_CELSIUS_NANO_SCALE


#define LPS22HB_MODE_DEFAULT			LPS22HB_NORMAL_MODE

#define LPS22HB_INT_FSS5_MASK			0x20
#define LPS22HB_INT_FTH_MASK			0x10
#define LPS22HB_INT_FOVR			0x08
#define LPS22HB_INT_DRDY_MASK			0x04
#define LPS22HB_INT_DATASIG2_MASK		0x02
#define LPS22HB_INT_DATASIG1_MASK		0x01

#define LPS22HB_BDU_ADDR			LPS22HB_CTRL1_ADDR
#define LPS22HB_BDU_MASK			0x02
#define LPS22HB_SOFT_RESET_ADDR			LPS22HB_CTRL2_ADDR
#define LPS22HB_SOFT_RESET_MASK			0x40
#define LPS22HB_LIR_ADDR			LPS22HB_INTERRUPT_CFG_ADDR
#define LPS22HB_LIR_MASK			0x04

#define LPS22HB_FIFO_MODE_ADDR			LPS22HB_FIFO_CTRL_ADDR
#define LPS22HB_FIFO_MODE_MASK			0xe0
#define LPS22HB_FIFO_MODE_BYPASS		0x00
#define LPS22HB_FIFO_MODE_STREAM		0x06

#define LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS	3
#define LPS22HB_FIFO_BYTE_FOR_SAMPLE_TEMP	2
#define LPS22HB_FIFO_BYTE_FOR_SAMPLE		(LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS + \
						 LPS22HB_FIFO_BYTE_FOR_SAMPLE_TEMP)

#define LPS22HB_TIMESTAMP_SIZE			8

#define LPS22HB_INT_STATUS_ADDR			0x25
#define LPS22HB_FIFO_SRC_ADDR			0x26
#define LPS22HB_STATUS_ADDR			0x27
#define LPS22HB_FIFO_STATUS_ADDR		LPS22HB_FIFO_SRC_ADDR
#define LPS22HB_FIFO_SRC_DIFF_MASK		0x3F
#define LPS22HB_FIFO_SRC_FTH_MASK		0x80

#define LPS22HB_EN_BIT				0x01
#define LPS22HB_DIS_BIT				0x00
#define LPS22HB_PRESS_ODR			1
#define LPS22HB_TEMP_ODR			1

#define LPS22HB_MAX_FIFO_LENGHT			32
#define LPS22HB_MAX_CHANNEL_SPEC		2
#define LPS22HB_PRESS_CHANNEL_SIZE		2
#define LPS22HB_TEMP_CHANNEL_SIZE		2
#define LPS22HB_EVENT_CHANNEL_SPEC_SIZE		2

#define LPS22HB_DEV_NAME			"lps22hb"
#define SET_BIT(a, b)				{a |= (1 << b);}
#define RESET_BIT(a, b)				{a &= ~(1 << b);}
#define CHECK_BIT(a, b)				(a & (1 << b))

enum {
	LPS22HB_PRESS = 0,
	LPS22HB_TEMP,
	LPS22HB_SENSORS_NUMB,
};

enum fifo_mode {
	BYPASS = 0,
	STREAM,
};

static inline s64 lps22hb_get_time_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

#define LPS22HB_TX_MAX_LENGTH		12
#define LPS22HB_RX_MAX_LENGTH		8193

struct lps22hb_transfer_buffer {
	struct mutex buf_lock;
	u8 rx_buf[LPS22HB_RX_MAX_LENGTH];
	u8 tx_buf[LPS22HB_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct lps22hb_data;

struct lps22hb_transfer_function {
	int (*write)(struct lps22hb_data *cdata, u8 reg_addr, int len, u8 *data);
	int (*read)(struct lps22hb_data *cdata, u8 reg_addr, int len, u8 *data);
};

struct lps22hb_sensor_data {
	struct lps22hb_data *cdata;
	const char *name;
	s64 timestamp;
	u8 enabled;
	u32 odr;
	u32 gainP;
	u32 gainT;
	u8 sindex;
	u8 sample_to_discard;
};

struct lps22hb_data {
	const char *name;
	u8 drdy_int_pin;
	u8 power_mode;
	u8 enabled_sensor;
	u32 common_odr;
	int irq;
	s64 timestamp, delta_ts;
	s64 sensor_timestamp;
	u8 *fifo_data;
	u16 fifo_size;
	struct device *dev;
	struct iio_dev *iio_sensors_dev[LPS22HB_SENSORS_NUMB];
	struct iio_trigger *iio_trig[LPS22HB_SENSORS_NUMB];
	const struct lps22hb_transfer_function *tf;
	struct lps22hb_transfer_buffer tb;
};

int lps22hb_common_probe(struct lps22hb_data *cdata, int irq);
#ifdef CONFIG_PM
int lps22hb_common_suspend(struct lps22hb_data *cdata);
int lps22hb_common_resume(struct lps22hb_data *cdata);
#endif
int lps22hb_allocate_rings(struct lps22hb_data *cdata);
int lps22hb_allocate_triggers(struct lps22hb_data *cdata,
			     const struct iio_trigger_ops *trigger_ops);
int lps22hb_trig_set_state(struct iio_trigger *trig, bool state);
int lps22hb_read_register(struct lps22hb_data *cdata, u8 reg_addr, int data_len,
							u8 *data);
int lps22hb_update_drdy_irq(struct lps22hb_sensor_data *sdata, bool state);
int lps22hb_set_enable(struct lps22hb_sensor_data *sdata, bool enable);
int lps22hb_update_fifo_ths(struct lps22hb_data *cdata);
void lps22hb_common_remove(struct lps22hb_data *cdata, int irq);
void lps22hb_read_fifo(struct lps22hb_data *cdata, bool check_fifo_len);
void lps22hb_deallocate_rings(struct lps22hb_data *cdata);
void lps22hb_deallocate_triggers(struct lps22hb_data *cdata);
int lps22hb_set_fifo_mode(struct lps22hb_data *cdata, enum fifo_mode fm);

#endif /* __LPS22HB_H */
