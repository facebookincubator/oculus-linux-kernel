/*
 * STMicroelectronics hts221 sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef HTS221_H
#define HTS221_H

#define HTS221_DEV_NAME		"hts221"

#include <linux/iio/iio.h>

#if defined(CONFIG_IIO_HTS221_SPI) || \
	defined(CONFIG_IIO_HTS221_SPI_MODULE)
#define HTS221_RX_MAX_LENGTH	500
#define HTS221_TX_MAX_LENGTH	500

struct hts221_transfer_buffer {
	u8 rx_buf[HTS221_RX_MAX_LENGTH];
	u8 tx_buf[HTS221_TX_MAX_LENGTH] ____cacheline_aligned;
};
#endif /* CONFIG_IIO_HTS221_SPI */

struct hts221_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

#define HTS221_AVG_DEPTH	8
struct hts221_avg_avl {
	u16 avg;
	u8 val;
};

enum hts221_sensor_type {
	HTS221_SENSOR_H,
	HTS221_SENSOR_T,
	HTS221_SENSOR_MAX,
};

struct hts221_sensor {
	struct hts221_dev *dev;
	struct iio_trigger *trig;

	enum hts221_sensor_type type;
	bool enabled;
	u8 odr, cur_avg_idx;
	int slope, b_gen;

	u8 drdy_data_mask;
	u8 buffer[2];
};

struct hts221_dev {
	const char *name;
	struct device *dev;
	int irq;
	struct mutex lock;

	struct iio_dev *iio_devs[HTS221_SENSOR_MAX];

	s64 hw_timestamp;

	const struct hts221_transfer_function *tf;
#if defined(CONFIG_IIO_HTS221_SPI) || \
	defined(CONFIG_IIO_HTS221_SPI_MODULE)
	struct hts221_transfer_buffer tb;
#endif /* CONFIG_IIO_HTS221_SPI */
};

static inline s64 hts221_get_time_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	return timespec_to_ns(&ts);
}

int hts221_config_drdy(struct hts221_dev *dev, bool enable);
int hts221_probe(struct hts221_dev *dev);
int hts221_remove(struct hts221_dev *dev);
int hts221_sensor_power_on(struct hts221_sensor *sensor);
int hts221_sensor_power_off(struct hts221_sensor *sensor);
#ifdef CONFIG_IIO_BUFFER
int hts221_allocate_buffers(struct hts221_dev *dev);
void hts221_deallocate_buffers(struct hts221_dev *dev);
int hts221_allocate_triggers(struct hts221_dev *dev);
void hts221_deallocate_triggers(struct hts221_dev *dev);
#else
static inline int hts221_allocate_buffers(struct hts221_dev *dev)
{
	return 0;
}

static inline void hts221_deallocate_buffers(struct hts221_dev *dev)
{
}

static inline int hts221_allocate_triggers(struct hts221_dev *dev)
{
	return 0;
}

static inline void hts221_deallocate_triggers(struct hts221_dev *dev)
{
}
#endif /* CONFIG_IIO_BUFFER */

#endif /* HTS221_H */
