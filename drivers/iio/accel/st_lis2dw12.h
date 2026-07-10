/*
 * STMicroelectronics lis2dw12 driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_LIS2DW12_H
#define ST_LIS2DW12_H

#include <linux/device.h>
#include <linux/iio/events.h>
#include <linux/iio/iio.h>

#define ST_LIS2DW12_DEV_NAME		"lis2dw12"
#define ST_LIS2DW12_MAX_WATERMARK	31
#define ST_LIS2DW12_DATA_SIZE		6

struct st_lis2dw12_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

#define ST_LIS2DW12_RX_MAX_LENGTH	8
#define ST_LIS2DW12_TX_MAX_LENGTH	8

struct st_lis2dw12_transfer_buffer {
	u8 rx_buf[ST_LIS2DW12_RX_MAX_LENGTH];
	u8 tx_buf[ST_LIS2DW12_TX_MAX_LENGTH] ____cacheline_aligned;
};

enum st_lis2dw12_event_id {
	ST_LIS2DW12_EVT_TAP,
	ST_LIS2DW12_EVT_TAP_TAP,
	ST_LIS2DW12_EVT_WU,
};

enum st_lis2dw12_fifo_mode {
	ST_LIS2DW12_FIFO_BYPASS = 0x0,
	ST_LIS2DW12_FIFO_CONTINUOUS = 0x6,
};

enum st_lis2dw12_selftest_status {
	ST_LIS2DW12_ST_RESET,
	ST_LIS2DW12_ST_PASS,
	ST_LIS2DW12_ST_FAIL,
};

struct st_lis2dw12_hw {
	const char *name;
	struct device *dev;

	struct mutex lock;

	u8 event_mask;

	int irq;
	u8 watermark;

	u16 gain;
	u16 odr;

	s64 delta_ts;
	s64 ts;

	enum st_lis2dw12_selftest_status st_status;

	const struct st_lis2dw12_transfer_function *tf;
	struct st_lis2dw12_transfer_buffer tb;
};

static inline s64 st_lis2dw12_get_timestamp(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);
	return timespec_to_ns(&ts);
}

int st_lis2dw12_probe(struct iio_dev *iio_dev);
int st_lis2dw12_remove(struct iio_dev *iio_dev);
int st_lis2dw12_init_ring(struct st_lis2dw12_hw *hw);
int st_lis2dw12_deallocate_ring(struct iio_dev *iio_dev);
int st_lis2dw12_set_fifomode(struct st_lis2dw12_hw *hw,
			     enum st_lis2dw12_fifo_mode mode);
int st_lis2dw12_read_fifo(struct st_lis2dw12_hw *hw, bool flush);
int st_lis2dw12_sensor_set_enable(struct st_lis2dw12_hw *hw, bool enable);

#endif /* ST_LIS2DW12_H */

