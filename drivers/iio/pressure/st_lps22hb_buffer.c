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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>

#include "st_lps22hb_core.h"

#define LPS22HB_PRESS_BUFFER_SIZE \
		ALIGN(LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS + LPS22HB_TIMESTAMP_SIZE, \
		      LPS22HB_TIMESTAMP_SIZE)
#define LPS22HB_TEMP_BUFFER_SIZE \
		ALIGN(LPS22HB_FIFO_BYTE_FOR_SAMPLE_TEMP + LPS22HB_TIMESTAMP_SIZE, \
		      LPS22HB_TIMESTAMP_SIZE)

static void lps22hb_push_fifo_data(struct lps22hb_data *cdata, u16 read_length)
{
	size_t offset;
	uint16_t k;
	u8 bufferP[LPS22HB_PRESS_BUFFER_SIZE], out_buf_indexP;
	u8 bufferT[LPS22HB_TEMP_BUFFER_SIZE], out_buf_indexT;
	struct iio_dev *indio_devP = cdata->iio_sensors_dev[LPS22HB_PRESS];
	struct iio_dev *indio_devT = cdata->iio_sensors_dev[LPS22HB_TEMP];
	struct lps22hb_sensor_data *sdata;

	out_buf_indexP = 0;
	out_buf_indexT = 0;

	/* Pressure data */
	sdata = iio_priv(indio_devP);
	if (sdata->enabled) {
		k = 0;

		if (indio_devP->active_scan_mask &&
						test_bit(0, indio_devP->active_scan_mask)) {
			memcpy(&bufferP[out_buf_indexP], &cdata->fifo_data[k],
									LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS);
			out_buf_indexP += LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS;
		}

		if (indio_devP->scan_timestamp) {
			offset = indio_devP->scan_bytes / sizeof(s64) - 1;
			((s64 *)bufferP)[offset] = cdata->sensor_timestamp;
		}

		iio_push_to_buffers(indio_devP, bufferP);
	}

	/* Temperature data */
	sdata = iio_priv(indio_devT);
	if (sdata->enabled) {
		k = LPS22HB_FIFO_BYTE_FOR_SAMPLE_PRESS;

		if (indio_devT->active_scan_mask &&
					test_bit(0, indio_devT->active_scan_mask)) {
			memcpy(&bufferT[out_buf_indexT], &cdata->fifo_data[k],
					LPS22HB_FIFO_BYTE_FOR_SAMPLE_TEMP);
			out_buf_indexT += LPS22HB_FIFO_BYTE_FOR_SAMPLE_TEMP;
		}

		if (indio_devT->scan_timestamp) {
			offset = indio_devT->scan_bytes / sizeof(s64) - 1;
			((s64 *)bufferT)[offset] = cdata->sensor_timestamp;
		}

		iio_push_to_buffers(indio_devT, bufferT);
	}
}

void lps22hb_read_fifo(struct lps22hb_data *cdata, bool check_fifo_len)
{
	int err;
	u8 fifo_src;
	u16 read_len = cdata->fifo_size;
	uint16_t i;
	u32 delta = cdata->iio_sensors_dev[LPS22HB_PRESS]->buffer->length;

	if (!cdata->fifo_data)
		return;

	if (check_fifo_len) {
		err = lps22hb_read_register(cdata, LPS22HB_FIFO_SRC_ADDR, 1, &fifo_src);
		if (err < 0)
			return;

		read_len = (fifo_src & LPS22HB_FIFO_SRC_DIFF_MASK);
		if (read_len)
			delta = div_s64(cdata->delta_ts, read_len);
		read_len *= LPS22HB_FIFO_BYTE_FOR_SAMPLE;

		if (read_len > cdata->fifo_size)
			read_len = cdata->fifo_size;
	}

	for (i = 0; i < read_len; i += LPS22HB_FIFO_BYTE_FOR_SAMPLE) {
		err = lps22hb_read_register(cdata, LPS22HB_PRESS_OUT_XL_ADDR,
					    LPS22HB_FIFO_BYTE_FOR_SAMPLE,
					    cdata->fifo_data);
		if (err < 0)
			return;

		lps22hb_push_fifo_data(cdata, LPS22HB_FIFO_BYTE_FOR_SAMPLE);
		cdata->sensor_timestamp += delta;
	}
}

static inline irqreturn_t lps22hb_handler_empty(int irq, void *p)
{
	return IRQ_HANDLED;
}

int lps22hb_trig_set_state(struct iio_trigger *trig, bool state)
{
	return 0;
}

static int lps22hb_buffer_preenable(struct iio_dev *indio_dev)
{
	int err;
	struct lps22hb_sensor_data *sdata = iio_priv(indio_dev);

	if (indio_dev->buffer->length >= LPS22HB_MAX_FIFO_LENGHT)
		indio_dev->buffer->length = LPS22HB_MAX_FIFO_LENGHT - 1;

	err = lps22hb_update_fifo_ths(sdata->cdata);
	if (err < 0)
		return err;

	lps22hb_set_fifo_mode(sdata->cdata, STREAM);

	err = lps22hb_set_enable(sdata, true);
	if (err < 0)
		return err;

	return 0;
}

static int lps22hb_buffer_postdisable(struct iio_dev *indio_dev)
{
	int err;
	struct lps22hb_sensor_data *sdata = iio_priv(indio_dev);

	err = lps22hb_set_enable(sdata, false);
	if (err < 0)
		return err;

	return 0;
}

static const struct iio_buffer_setup_ops lps22hb_buffer_setup_ops = {
	.preenable = &lps22hb_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
	.postdisable = &lps22hb_buffer_postdisable,
};

int lps22hb_allocate_rings(struct lps22hb_data *cdata)
{
	int err, i;

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++) {
		err = iio_triggered_buffer_setup(
				cdata->iio_sensors_dev[i],
				&lps22hb_handler_empty,
				NULL,
				&lps22hb_buffer_setup_ops);
		if (err < 0)
			goto buffer_cleanup;
	}

	return 0;

buffer_cleanup:
	for (i--; i >= 0; i--)
		iio_triggered_buffer_cleanup(cdata->iio_sensors_dev[i]);

	return err;
}

void lps22hb_deallocate_rings(struct lps22hb_data *cdata)
{
	int i;

	for (i = 0; i < LPS22HB_SENSORS_NUMB; i++)
		iio_triggered_buffer_cleanup(cdata->iio_sensors_dev[i]);
}

MODULE_DESCRIPTION("STMicroelectronics lps22hb driver");
MODULE_AUTHOR("Armando Visconti <armando.visconti@st.com>");
MODULE_LICENSE("GPL v2");
