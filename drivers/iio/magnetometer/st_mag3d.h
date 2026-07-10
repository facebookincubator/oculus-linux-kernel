/*
 * STMicroelectronics st_mag3d driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef __ST_MAG3D_H
#define __ST_MAG3D_H

#define LIS3MDL_DEV_NAME		"lis3mdl_magn"
#define LSM9DS1_DEV_NAME		"lsm9ds1_magn"

#define ST_MAG3D_TX_MAX_LENGTH		16
#define ST_MAG3D_RX_MAX_LENGTH		16

#define ST_MAG3D_SAMPLE_SIZE		6

struct iio_dev;

struct st_mag3d_transfer_buffer {
	u8 rx_buf[ST_MAG3D_RX_MAX_LENGTH];
	u8 tx_buf[ST_MAG3D_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct st_mag3d_transfer_function {
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
};

struct st_mag3d_hw {
	struct device *dev;
	struct mutex lock;

	u16 odr;
	u16 gain;
	s64 timestamp;

	struct iio_trigger *trig;
	int irq;

	const struct st_mag3d_transfer_function *tf;
	struct st_mag3d_transfer_buffer tb;
};

int st_mag3d_probe(struct device *dev, int irq, const char *name,
		   const struct st_mag3d_transfer_function *tf_ops);
void st_mag3d_remove(struct iio_dev *iio_dev);
int st_mag3d_allocate_buffer(struct iio_dev *iio_dev);
void st_mag3d_deallocate_buffer(struct iio_dev *iio_dev);
int st_mag3d_allocate_trigger(struct iio_dev *iio_dev);
void st_mag3d_deallocate_trigger(struct iio_dev *iio_dev);
int st_mag3d_enable_sensor(struct st_mag3d_hw *hw, bool enable);

#endif /* __ST_MAG3D_H */
