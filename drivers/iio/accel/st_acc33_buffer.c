/*
 * STMicroelectronics st_acc33 sensor driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/iio/kfifo_buf.h>

#include "st_acc33.h"

static irqreturn_t st_acc33_ring_handler_irq(int irq, void *private)
{
	struct st_acc33_dev *dev = (struct st_acc33_dev *)private;
	s64 ts;

	ts = st_acc33_get_time_ns();
	dev->delta_ts = ts - dev->ts;
	dev->ts = ts;

	return IRQ_WAKE_THREAD;
}

static irqreturn_t st_acc33_ring_handler_thread(int irq, void *private)
{
	struct st_acc33_dev *dev = (struct st_acc33_dev *)private;

	st_acc33_read_hwfifo(dev, false);

	return IRQ_HANDLED;
}

static int st_acc33_buffer_preenable(struct iio_dev *iio_dev)
{
	return st_acc33_set_enable(iio_priv(iio_dev), true);
}

static int st_acc33_buffer_postdisable(struct iio_dev *iio_dev)
{
	return st_acc33_set_enable(iio_priv(iio_dev), false);
}

static const struct iio_buffer_setup_ops st_acc33_buffer_ops = {
	.preenable = st_acc33_buffer_preenable,
	.postdisable = st_acc33_buffer_postdisable,
};

int st_acc33_init_ring(struct st_acc33_dev *dev)
{
	struct iio_dev *iio_dev = iio_priv_to_dev(dev);
	struct iio_buffer *buffer;
	int ret;

	ret = devm_request_threaded_irq(dev->dev, dev->irq,
					st_acc33_ring_handler_irq,
					st_acc33_ring_handler_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					dev->name, dev);
	if (ret) {
		dev_err(dev->dev, "failed to request trigger irq %d\n",
			dev->irq);
		return ret;
	}

	buffer = iio_kfifo_allocate(iio_dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(iio_dev, buffer);
	iio_dev->setup_ops = &st_acc33_buffer_ops;
	iio_dev->modes |= INDIO_BUFFER_HARDWARE;

	ret = iio_buffer_register(iio_dev, iio_dev->channels,
				  iio_dev->num_channels);
	if (ret) {
		iio_kfifo_free(iio_dev->buffer);
		return ret;
	}

	return 0;
}

void st_acc33_deallocate_ring(struct st_acc33_dev *dev)
{
	struct iio_dev *iio_dev = iio_priv_to_dev(dev);

	iio_buffer_unregister(iio_dev);
	iio_kfifo_free(iio_dev->buffer);
}

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_acc33 buffer driver");
MODULE_LICENSE("GPL v2");
