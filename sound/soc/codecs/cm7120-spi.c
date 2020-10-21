/*
 * cm7120-spi.c  --  ALC5679 ALSA SoC audio codec driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_qos.h>
#include <linux/sysfs.h>
#include <linux/clk.h>
#include "cm7120-spi.h"

static struct spi_device *cm7120_spi;

int cm7120_spi_read(u32 addr, unsigned int *val, size_t len)
{
	struct spi_device *spi = cm7120_spi;
	struct spi_message message;
	struct spi_transfer x[3];
	int status;
	u8 write_buf[5];
	u8 read_buf[4];

	write_buf[0] =
		(len == 4) ? CM7120_SPI_CMD_32_READ : CM7120_SPI_CMD_16_READ;
	write_buf[1] = (addr & 0xff000000) >> 24;
	write_buf[2] = (addr & 0x00ff0000) >> 16;
	write_buf[3] = (addr & 0x0000ff00) >> 8;
	write_buf[4] = (addr & 0x000000ff) >> 0;

	spi_message_init(&message);
	memset(x, 0, sizeof(x));

	x[0].len = 5;
	x[0].tx_buf = write_buf;
	spi_message_add_tail(&x[0], &message);

	x[1].len = 4;
	x[1].tx_buf = write_buf;
	spi_message_add_tail(&x[1], &message);

	x[2].len = len;
	x[2].rx_buf = read_buf;
	spi_message_add_tail(&x[2], &message);

	status = spi_sync(spi, &message);

	if (len == 4)
		*val = read_buf[3] | read_buf[2] << 8 | read_buf[1] << 16 |
		       read_buf[0] << 24;
	else
		*val = read_buf[1] | read_buf[0] << 8;

	return status;
}
EXPORT_SYMBOL_GPL(cm7120_spi_read);

int cm7120_spi_write(u32 addr, unsigned int val, size_t len)
{
	struct spi_device *spi = cm7120_spi;
	int status;
	u8 write_buf[10];

	write_buf[1] = (addr & 0xff000000) >> 24;
	write_buf[2] = (addr & 0x00ff0000) >> 16;
	write_buf[3] = (addr & 0x0000ff00) >> 8;
	write_buf[4] = (addr & 0x000000ff) >> 0;

	if (len == 4) {
		write_buf[0] = CM7120_SPI_CMD_32_WRITE;
		write_buf[5] = (val & 0xff000000) >> 24;
		write_buf[6] = (val & 0x00ff0000) >> 16;
		write_buf[7] = (val & 0x0000ff00) >> 8;
		write_buf[8] = (val & 0x000000ff) >> 0;
	} else {
		write_buf[0] = CM7120_SPI_CMD_16_WRITE;
		write_buf[5] = (val & 0x0000ff00) >> 8;
		write_buf[6] = (val & 0x000000ff) >> 0;
	}

	status = spi_write(spi, write_buf,
			   (len == 4) ? sizeof(write_buf) :
					sizeof(write_buf) - 2);

	if (status)
		dev_err(&spi->dev, "%s error %d\n", __func__, status);

	return status;
}
EXPORT_SYMBOL_GPL(cm7120_spi_write);

/**
 * cm7120_spi_burst_read - Read data from SPI by cm7120 dsp memory address.
 * @addr: Start address.
 * @rxbuf: Data Buffer for reading.
 * @len: Data length, it must be a multiple of 8.
 *
 *
 * Returns true for success.
 */
int cm7120_spi_burst_read(u32 addr, u8 *rxbuf, size_t len)
{
	u8 spi_cmd = CM7120_SPI_CMD_BURST_READ;
	int status;
	u8 write_buf[8];
	unsigned int i, end, offset = 0;

	struct spi_message message;
	struct spi_transfer x[3];

	while (offset < len) {
		if (offset + CM7120_SPI_BUF_LEN <= len)
			end = CM7120_SPI_BUF_LEN;
		else
			end = len % CM7120_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0xff000000) >> 24;
		write_buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		spi_message_init(&message);
		memset(x, 0, sizeof(x));

		x[0].len = 5;
		x[0].tx_buf = write_buf;
		spi_message_add_tail(&x[0], &message);

		x[1].len = 4;
		x[1].tx_buf = write_buf;
		spi_message_add_tail(&x[1], &message);

		x[2].len = end;
		x[2].rx_buf = rxbuf + offset;
		spi_message_add_tail(&x[2], &message);

		status = spi_sync(cm7120_spi, &message);

		if (status)
			return false;

		offset += CM7120_SPI_BUF_LEN;
	}

	for (i = 0; i < len; i += 8) {
		write_buf[0] = rxbuf[i + 0];
		write_buf[1] = rxbuf[i + 1];
		write_buf[2] = rxbuf[i + 2];
		write_buf[3] = rxbuf[i + 3];
		write_buf[4] = rxbuf[i + 4];
		write_buf[5] = rxbuf[i + 5];
		write_buf[6] = rxbuf[i + 6];
		write_buf[7] = rxbuf[i + 7];

		rxbuf[i + 0] = write_buf[7];
		rxbuf[i + 1] = write_buf[6];
		rxbuf[i + 2] = write_buf[5];
		rxbuf[i + 3] = write_buf[4];
		rxbuf[i + 4] = write_buf[3];
		rxbuf[i + 5] = write_buf[2];
		rxbuf[i + 6] = write_buf[1];
		rxbuf[i + 7] = write_buf[0];
	}

	return true;
}
EXPORT_SYMBOL_GPL(cm7120_spi_burst_read);

/**
 * cm7120_spi_burst_write - Write data to SPI by cm7120 dsp memory address.
 * @addr: Start address.
 * @txbuf: Data Buffer for writng.
 * @len: Data length, it must be a multiple of 8.
 *
 *
 * Returns true for success.
 */
int cm7120_spi_burst_write(u32 addr, const u8 *txbuf, size_t len)
{
	u8 spi_cmd = CM7120_SPI_CMD_BURST_WRITE;
	u8 *write_buf;
	unsigned int i, end, offset = 0;
	int status;

	write_buf = kmalloc(CM7120_SPI_BUF_LEN + 6, GFP_KERNEL);

	if (write_buf == NULL)
		return -ENOMEM;

	while (offset < len) {
		if (offset + CM7120_SPI_BUF_LEN <= len)
			end = CM7120_SPI_BUF_LEN;
		else
			end = len % CM7120_SPI_BUF_LEN;

		write_buf[0] = spi_cmd;
		write_buf[1] = ((addr + offset) & 0xff000000) >> 24;
		write_buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		write_buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		write_buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		for (i = 0; i < end; i += 8) {
			write_buf[i + 12] = txbuf[offset + i + 0];
			write_buf[i + 11] = txbuf[offset + i + 1];
			write_buf[i + 10] = txbuf[offset + i + 2];
			write_buf[i + 9] = txbuf[offset + i + 3];
			write_buf[i + 8] = txbuf[offset + i + 4];
			write_buf[i + 7] = txbuf[offset + i + 5];
			write_buf[i + 6] = txbuf[offset + i + 6];
			write_buf[i + 5] = txbuf[offset + i + 7];
		}

		write_buf[end + 5] = spi_cmd;

		status = spi_write(cm7120_spi, write_buf, end + 6);

		if (status) {
			dev_err(&cm7120_spi->dev, "%s error %d\n", __func__,
				status);
			kfree(write_buf);
			return status;
		}

		offset += CM7120_SPI_BUF_LEN;
	}

	kfree(write_buf);

	return 0;
}
EXPORT_SYMBOL_GPL(cm7120_spi_burst_write);

static int cm7120_spi_probe(struct spi_device *spi)
{
	pr_info("%s entry\n", __func__);

	cm7120_spi = spi;
	return 0;
}

#if defined(CONFIG_OF)
static const struct of_device_id cm7120_of_match[] = {
	{.compatible = "C-Media,cm7120-spi",},
	{},
};
MODULE_DEVICE_TABLE(of, cm7120_of_match);
#endif

static struct spi_driver cm7120_spi_driver = {
	.driver = {
			.name = "cm7120",
			.of_match_table = cm7120_of_match,
	},
	.probe = cm7120_spi_probe,
};
module_spi_driver(cm7120_spi_driver);

MODULE_DESCRIPTION("CM7120 SPI driver");
MODULE_AUTHOR("Tzung-Dar Tsai <tdtsai@cmedia.com.tw>");
MODULE_LICENSE("GPL v2");
