/*
 * STMicroelectronics st_acc33 sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_ACC33_H
#define ST_ACC33_H

#define LIS2DH_DEV_NAME		"lis2dh_accel"
#define LIS3DH_DEV_NAME		"lis3dh_accel"
#define LSM303AGR_DEV_NAME	"lsm303agr_accel"

#include <linux/iio/iio.h>

#define ST_ACC33_SAMPLE_FREQ_ATTR()				\
	IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,		\
			       st_acc33_get_sampling_frequency, \
			       st_acc33_set_sampling_frequency)

#define ST_ACC33_SAMPLE_FREQ_AVAIL_ATTR()			\
	IIO_DEV_ATTR_SAMP_FREQ_AVAIL(st_acc33_get_sampling_frequency_avail)

#define ST_ACC33_SCALE_AVAIL_ATTR(name)				\
	IIO_DEVICE_ATTR(name, S_IRUGO, st_acc33_get_scale_avail, NULL, 0)

#define ST_ACC33_HWFIFO_ENABLED()				\
	IIO_DEVICE_ATTR(hwfifo_enabled, S_IWUSR | S_IRUGO,	\
			st_acc33_get_hwfifo_enabled,		\
			st_acc33_set_hwfifo_enabled, 0);

#define ST_ACC33_HWFIFO_WATERMARK()				\
	IIO_DEVICE_ATTR(hwfifo_watermark, S_IWUSR | S_IRUGO,	\
			st_acc33_get_hwfifo_watermark,		\
			st_acc33_set_hwfifo_watermark, 0);

#define ST_ACC33_HWFIFO_WATERMARK_MIN()				\
	IIO_DEVICE_ATTR(hwfifo_watermark_min, S_IRUGO,		\
			st_acc33_get_min_hwfifo_watermark,	\
			NULL, 0);

#define ST_ACC33_HWFIFO_WATERMARK_MAX()				\
	IIO_DEVICE_ATTR(hwfifo_watermark_max, S_IRUGO,		\
			st_acc33_get_max_hwfifo_watermark,	\
			NULL, 0);

#define ST_ACC33_HWFIFO_FLUSH()					\
	IIO_DEVICE_ATTR(hwfifo_flush, S_IWUSR, NULL,		\
			st_acc33_flush_hwfifo, 0);

#define ST_ACC33_DATA_CHANNEL(addr, modx, scan_idx)		\
{								\
	.type = IIO_ACCEL,					\
	.address = addr,					\
	.modified = 1,						\
	.channel2 = modx,					\
	.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |		\
			      BIT(IIO_CHAN_INFO_SCALE),		\
	.scan_index = scan_idx,					\
	.scan_type = {						\
		.sign = 's',					\
		.realbits = 12,					\
		.storagebits = 16,				\
		.shift = 4,					\
		.endianness = IIO_LE,				\
	},							\
}

#define ST_ACC33_FLUSH_CHANNEL()				\
{								\
	.type = IIO_ACCEL,					\
	.modified = 0,						\
	.scan_index = -1,					\
	.indexed = -1,						\
	.event_spec = &st_acc33_fifo_flush_event,		\
	.num_event_specs = 1,					\
}

#if defined(CONFIG_IIO_ST_ACC33_SPI) || \
	defined(CONFIG_IIO_ST_ACC33_SPI_MODULE)
#define ST_ACC33_RX_MAX_LENGTH	500
#define ST_ACC33_TX_MAX_LENGTH	500

struct st_acc33_transfer_buffer {
	u8 rx_buf[ST_ACC33_RX_MAX_LENGTH];
	u8 tx_buf[ST_ACC33_TX_MAX_LENGTH] ____cacheline_aligned;
};
#endif /* CONFIG_IIO_ST_ACC33_SPI */

struct st_acc33_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

enum st_acc33_fifo_mode {
	ST_ACC33_FIFO_BYPASS = 0x0,
	ST_ACC33_FIFO_STREAM = 0x2,
};

struct st_acc33_dev {
	const char *name;
	struct device *dev;
	int irq;
	s64 ts, delta_ts;
	struct mutex lock;

	u8 watermark;

	u16 odr;
	u32 gain;

	const struct st_acc33_transfer_function *tf;
#if defined(CONFIG_IIO_ST_ACC33_SPI) || \
	defined(CONFIG_IIO_ST_ACC33_SPI_MODULE)
	struct st_acc33_transfer_buffer tb;
#endif /* CONFIG_IIO_ST_ACC33_SPI */
};

static inline s64 st_acc33_get_time_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

int st_acc33_read_hwfifo(struct st_acc33_dev *dev, bool flush);
int st_acc33_set_enable(struct st_acc33_dev *dev, bool enable);
int st_acc33_probe(struct st_acc33_dev *dev);
void st_acc33_remove(struct st_acc33_dev *dev);
int st_acc33_init_ring(struct st_acc33_dev *dev);
void st_acc33_deallocate_ring(struct st_acc33_dev *dev);

#endif /* ST_ACC33_H */
