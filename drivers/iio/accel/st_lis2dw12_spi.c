/*
 * STMicroelectronics lis2dw12 spi driver
 *
 * Copyright 2016 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/of.h>

#include "st_lis2dw12.h"

#define SENSORS_SPI_READ	0x80

static int st_lis2dw12_spi_read(struct device *dev, u8 addr, int len, u8 *data)
{
	struct spi_device *spi = to_spi_device(dev);
	struct iio_dev *iio_dev = spi_get_drvdata(spi);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);
	int err;

	struct spi_transfer xfers[] = {
		{
			.tx_buf = hw->tb.tx_buf,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = hw->tb.rx_buf,
			.bits_per_word = 8,
			.len = len,
		}
	};

	hw->tb.tx_buf[0] = addr | SENSORS_SPI_READ;

	err = spi_sync_transfer(spi, xfers,  ARRAY_SIZE(xfers));
	if (err < 0)
		return err;

	memcpy(data, hw->tb.rx_buf, len * sizeof(u8));

	return len;
}

static int st_lis2dw12_spi_write(struct device *dev, u8 addr, int len,
				 u8 *data)
{
	struct spi_device *spi = to_spi_device(dev);
	struct iio_dev *iio_dev = spi_get_drvdata(spi);
	struct st_lis2dw12_hw *hw = iio_priv(iio_dev);

	struct spi_transfer xfers = {
		.tx_buf = hw->tb.tx_buf,
		.bits_per_word = 8,
		.len = len + 1,
	};

	if (len >= ST_LIS2DW12_TX_MAX_LENGTH)
		return -ENOMEM;

	hw->tb.tx_buf[0] = addr;
	memcpy(&hw->tb.tx_buf[1], data, len);

	return spi_sync_transfer(spi, &xfers, 1);
}

static const struct st_lis2dw12_transfer_function st_lis2dw12_transfer_fn = {
	.read = st_lis2dw12_spi_read,
	.write = st_lis2dw12_spi_write,
};

static int st_lis2dw12_spi_probe(struct spi_device *spi)
{
	struct st_lis2dw12_hw *hw;
	struct iio_dev *iio_dev;

	iio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*hw));
	if (!iio_dev)
		return -ENOMEM;

	spi_set_drvdata(spi, iio_dev);

	hw = iio_priv(iio_dev);
	hw->name = spi->modalias;
	hw->dev = &spi->dev;
	hw->irq = spi->irq;
	hw->tf = &st_lis2dw12_transfer_fn;

	return st_lis2dw12_probe(iio_dev);
}

static int st_lis2dw12_spi_remove(struct spi_device *spi)
{
	struct iio_dev *iio_dev = spi_get_drvdata(spi);

	return st_lis2dw12_remove(iio_dev);
}

static const struct of_device_id st_lis2dw12_spi_of_match[] = {
	{
		.compatible = "st,lis2dw12",
		.data = ST_LIS2DW12_DEV_NAME,
	},
	{},
};
MODULE_DEVICE_TABLE(of, st_lis2dw12_spi_of_match);

static const struct spi_device_id st_lis2dw12_spi_id_table[] = {
	{ ST_LIS2DW12_DEV_NAME },
	{},
};
MODULE_DEVICE_TABLE(spi, st_lis2dw12_spi_id_table);

static struct spi_driver st_lis2dw12_driver = {
	.driver = {
		.name = "st_lis2dw12_spi",
		.of_match_table = of_match_ptr(st_lis2dw12_spi_of_match),
	},
	.probe = st_lis2dw12_spi_probe,
	.remove = st_lis2dw12_spi_remove,
	.id_table = st_lis2dw12_spi_id_table,
};
module_spi_driver(st_lis2dw12_driver);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics st_lis2dw12 spi driver");
MODULE_LICENSE("GPL v2");
