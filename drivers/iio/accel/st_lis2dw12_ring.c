/*
 * STMicroelectronics lis2dw12 fifo driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/iio/kfifo_buf.h>

#include "st_lis2dw12.h"

#define ST_LIS2DW12_STATUS_ADDR			0x27
#define ST_LIS2DW12_STATUS_FF_MASK		0x01
#define ST_LIS2DW12_STATUS_TAP_TAP_MASK		0x10
#define ST_LIS2DW12_STATUS_TAP_MASK		0x08
#define ST_LIS2DW12_STATUS_WU_MASK		0x40
#define ST_LIS2DW12_STATUS_FTH_MASK		0x80
#define ST_LIS2DW12_FIFO_SAMPLES_ADDR		0x2f
#define ST_LIS2DW12_FIFO_SAMPLES_FTH_MASK	0x80
#define ST_LIS2DW12_FIFO_SAMPLES_OVR_MASK	0x40
#define ST_LIS2DW12_WU_SRC_ADDR			0x38
#define ST_LIS2DW12_TAP_SRC_ADDR		0x39
#define ST_LIS2DW12_STAP_SRC_MASK		0x20
#define ST_LIS2DW12_DTAP_SRC_MASK		0x10
#define ST_LIS2DW12_TAP_EVT_MASK		0x07
#define ST_LIS2DW12_FIFO_SAMPLES_DIFF_MASK	0x3f

static int st_lis2dw12_fifo_preenable(struct iio_dev *iio_dev)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err;

	err = st_lis2dw12_set_fifomode(hw, ST_LIS2DW12_FIFO_CONTINUOUS);
	if (err < 0)
		return err;

	hw->ts = st_lis2dw12_get_timestamp();

	return st_lis2dw12_sensor_set_enable(hw, true);
}

static int st_lis2dw12_fifo_postdisable(struct iio_dev *iio_dev)
{
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err;

	err = st_lis2dw12_set_fifomode(hw, ST_LIS2DW12_FIFO_BYPASS);
	if (err < 0)
		return err;

	return st_lis2dw12_sensor_set_enable(hw, false);
}

static const struct iio_buffer_setup_ops st_lis2dw12_acc_buffer_setup_ops = {
	.preenable = st_lis2dw12_fifo_preenable,
	.postdisable = st_lis2dw12_fifo_postdisable,
};

static int st_lis2dw12_allocate_fifo(struct st_lis2dw12_hw *hw)
{
	struct iio_dev *iio_dev = iio_priv_to_dev(hw);
	struct iio_buffer *buffer;
	int err;

	buffer = iio_kfifo_allocate(iio_dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(iio_dev, buffer);

	iio_dev->setup_ops = &st_lis2dw12_acc_buffer_setup_ops;
	iio_dev->modes |= INDIO_BUFFER_HARDWARE;

	err = iio_buffer_register(iio_dev, iio_dev->channels,
				  iio_dev->num_channels);
	if (err) {
		iio_kfifo_free(iio_dev->buffer);
		return err;
	}

	return 0;
}

int st_lis2dw12_read_fifo(struct st_lis2dw12_hw *hw, bool flush)
{
	u8 data[ALIGN(ST_LIS2DW12_DATA_SIZE, sizeof(s64)) + sizeof(s64)];
	u8 status, samples;
	struct iio_dev *iio_dev = iio_priv_to_dev(hw);
	struct iio_chan_spec const *ch = iio_dev->channels;
	s64 ts = hw->ts, delta_ts;
	int i, err;

	err = hw->tf->read(hw->dev, ST_LIS2DW12_FIFO_SAMPLES_ADDR,
			   sizeof(status), &status);
	if (err < 0)
		return err;

	samples = status & ST_LIS2DW12_FIFO_SAMPLES_DIFF_MASK;
	if (!samples)
		return 0;

	delta_ts = flush ? div_s64(1000000000LL, hw->odr)
			 : div_s64(hw->delta_ts, samples);

	for (i = 0; i < samples; i++) {
		err = hw->tf->read(hw->dev, ch[0].address,
				   ST_LIS2DW12_DATA_SIZE, data);
		if (err < 0)
			return err;

		ts = hw->ts - (samples - 1 - i) * delta_ts;
		iio_push_to_buffers_with_timestamp(iio_dev, data, ts);
	}

	return samples;
}

static irqreturn_t st_lis2dw12_ring_handler_irq(int irq, void *private)
{
	struct st_lis2dw12_hw *hw = (struct st_lis2dw12_hw *)private;
	s64 ts;

	ts = st_lis2dw12_get_timestamp();
	hw->delta_ts = ts - hw->ts;
	hw->ts = ts;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_lis2dw12_ring_handler_thread(int irq, void *private)
{
	struct st_lis2dw12_hw *hw = (struct st_lis2dw12_hw *)private;
	struct iio_dev *iio_dev = iio_priv_to_dev(hw);
	u8 status;
	s64 code;
	int err;

	err = hw->tf->read(hw->dev, ST_LIS2DW12_STATUS_ADDR, sizeof(status),
			   &status);
	if (err < 0)
		goto out;

	if (status & ST_LIS2DW12_STATUS_FTH_MASK)
		st_lis2dw12_read_fifo(hw, false);

	if (((status & ST_LIS2DW12_STATUS_TAP_MASK) &&
	     (hw->event_mask & BIT(ST_LIS2DW12_EVT_TAP))) ||
	    ((status & ST_LIS2DW12_STATUS_TAP_TAP_MASK) &&
	     (hw->event_mask & BIT(ST_LIS2DW12_EVT_TAP_TAP)))) {
		enum iio_chan_type type;
		u8 tap_src;

		err = hw->tf->read(hw->dev, ST_LIS2DW12_TAP_SRC_ADDR,
				   sizeof(tap_src), &tap_src);
		if (err < 0)
			goto out;

		type = (tap_src & ST_LIS2DW12_DTAP_SRC_MASK) ? IIO_TAP_TAP
							     : IIO_TAP;
		code = IIO_UNMOD_EVENT_CODE(IIO_TAP_TAP, -1,
					    IIO_EV_TYPE_THRESH,
					    IIO_EV_DIR_RISING);
		iio_push_event(iio_dev, code, hw->ts);
	}

	if (status & ST_LIS2DW12_STATUS_WU_MASK) {
		u8 wu_src;

		err = hw->tf->read(hw->dev, ST_LIS2DW12_WU_SRC_ADDR,
				   sizeof(wu_src), &wu_src);
		if (err < 0)
			goto out;

		code = IIO_UNMOD_EVENT_CODE(IIO_TAP_TAP, -1,
					    IIO_EV_TYPE_THRESH,
					    IIO_EV_DIR_RISING);
		iio_push_event(iio_dev, IIO_TILT, hw->ts);
	}

out:
	return IRQ_HANDLED;
}

int st_lis2dw12_init_ring(struct st_lis2dw12_hw *hw)
{
	int ret;

	ret = devm_request_threaded_irq(hw->dev, hw->irq,
					st_lis2dw12_ring_handler_irq,
					st_lis2dw12_ring_handler_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					hw->name, hw);
	if (ret) {
		dev_err(hw->dev, "failed to request trigger irq %d\n",
			hw->irq);
		return ret;
	}

	ret = st_lis2dw12_allocate_fifo(hw);

	return ret < 0 ? ret : 0;
}

int st_lis2dw12_deallocate_ring(struct iio_dev *iio_dev)
{
	iio_buffer_unregister(iio_dev);
	iio_kfifo_free(iio_dev->buffer);

	return 0;
}
