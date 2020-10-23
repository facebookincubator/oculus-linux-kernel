/*
 * Driver for the CM710x codec
 *
 * Author:	Tzung-Dar Tsai <tdtsai@cmedia.com.tw>
 *		Copyright 2017
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/spi/spi.h>
#include "cm710x-spi.h"

#define CM710X_SPI_BURST_LEN	240
#define CM710X_SPI_HEADER	5
#define CM710X_SPI_FREQ		6000000

#define CM710X_SPI_WRITE_BURST	0x5
#define CM710X_SPI_READ_BURST	0x4
#define CM710X_SPI_WRITE_32	0x3
#define CM710X_SPI_READ_32	0x2
#define CM710X_SPI_WRITE_16	0x1
#define CM710X_SPI_READ_16	0x0

static struct spi_device *g_spi;
static DEFINE_MUTEX(spi_mutex);

static u8 cm710x_SPI_select_cmd(bool read, u32 align, u32 remain, u32 *len)
{
	u8 cmd;

	if (align == 2 || align == 6 || remain == 2) {
		cmd = CM710X_SPI_READ_16;
		*len = 2;
	} else if (align == 4 || remain <= 6) {
		cmd = CM710X_SPI_READ_32;
		*len = 4;
	} else {
		cmd = CM710X_SPI_READ_BURST;
		*len = min_t(u32, remain & ~7, CM710X_SPI_BURST_LEN);
	}
	return read ? cmd : cmd + 1;
}

static void cm710x_SPI_fix_order(u8 *dst, u32 dstlen, const u8 *src, u32 srclen)
{
	u32 w, i, si;
	u32 word_size = min_t(u32, dstlen, 8);

	for (w = 0; w < dstlen; w += word_size) {
		for (i = 0; i < word_size; i++) {
			si = w + word_size - i - 1;
			dst[w + i] = si < srclen ? src[si] : 0;
		}
	}
}

int cm710x_read_SPI_Dsp(u32 addr, u8 *Data, int len)
{
	u32 offset;
	int status = 0;
	struct spi_transfer t[2];
	struct spi_message m;
	/* +4 bytes is for the DummyPhase following the AddressPhase */
	u8 header[CM710X_SPI_HEADER + 4];
	u8 body[CM710X_SPI_BURST_LEN];
	u8 spi_cmd;
	u8 *cb = Data;

	if (!g_spi)
		return -ENODEV;

	if ((addr & 1) || (len & 1)) {
		dev_err(&g_spi->dev, "Bad read align 0x%x(%d)\n", addr, len);
		return -EACCES;
	}

	memset(t, 0, sizeof(t));
	t[0].tx_buf = header;
	t[0].len = sizeof(header);
	t[0].speed_hz = g_spi->max_speed_hz;
	t[1].rx_buf = body;
	t[1].speed_hz = g_spi->max_speed_hz;
	spi_message_init_with_transfers(&m, t, ARRAY_SIZE(t));

	for (offset = 0; offset < len; offset += t[1].len) {
		spi_cmd = cm710x_SPI_select_cmd(true, (addr + offset) & 7,
				len - offset, &t[1].len);

		/* Construct SPI message header */
		header[0] = spi_cmd;
		header[1] = ((addr + offset) & 0xff000000) >> 24;
		header[2] = ((addr + offset) & 0x00ff0000) >> 16;
		header[3] = ((addr + offset) & 0x0000ff00) >> 8;
		header[4] = ((addr + offset) & 0x000000ff) >> 0;

		mutex_lock(&spi_mutex);
		status |= spi_sync(g_spi, &m);
		mutex_unlock(&spi_mutex);

		/* Copy data back to caller buffer */
		cm710x_SPI_fix_order(cb + offset, t[1].len, body, t[1].len);
	}
	return status;
}
EXPORT_SYMBOL_GPL(cm710x_read_SPI_Dsp);

int cm710x_write_SPI_Dsp(u32 addr, u8 *Data, int len)
{
	u32 offset, len_with_pad = len;
	int status = 0;
	struct spi_transfer t;
	struct spi_message m;
	/* +1 byte is for the DummyPhase following the DataPhase */
	u8 buf[CM710X_SPI_HEADER + CM710X_SPI_BURST_LEN + 1];
	u8 *body = buf + CM710X_SPI_HEADER;
	u8 spi_cmd;
	u8 *cb = Data;

	if (!g_spi)
		return -ENODEV;

	if (addr & 1) {
		dev_err(&g_spi->dev, "Bad write align 0x%x(%d)\n", addr, len);
		return -EACCES;
	}

	if (len & 1)
		len_with_pad = len + 1;

	memset(&t, 0, sizeof(t));
	t.tx_buf = buf;
	t.speed_hz = g_spi->max_speed_hz;
	spi_message_init_with_transfers(&m, &t, 1);

	for (offset = 0; offset < len_with_pad;) {
		spi_cmd = cm710x_SPI_select_cmd(false, (addr + offset) & 7,
				len_with_pad - offset, &t.len);

		/* Construct SPI message header */
		buf[0] = spi_cmd;
		buf[1] = ((addr + offset) & 0xff000000) >> 24;
		buf[2] = ((addr + offset) & 0x00ff0000) >> 16;
		buf[3] = ((addr + offset) & 0x0000ff00) >> 8;
		buf[4] = ((addr + offset) & 0x000000ff) >> 0;

		/* Fetch data from caller buffer */
		cm710x_SPI_fix_order(body, t.len, cb + offset, len - offset);
		offset += t.len;
		t.len += CM710X_SPI_HEADER + 1;

		mutex_lock(&spi_mutex);
		status |= spi_sync(g_spi, &m);
		mutex_unlock(&spi_mutex);
	}
	return status;
}
EXPORT_SYMBOL_GPL(cm710x_write_SPI_Dsp);

static const struct of_device_id cm710x_of_match[] = {
	{ .compatible = "C-Media,cm710x-spi", },
	{ }
};

MODULE_DEVICE_TABLE(of, cm710x_of_match);

static void cm710x_parse_dt(struct spi_device *spi)
{
	u32 temp;
	int rc;

	rc = of_property_read_u32(spi->dev.of_node,
				"spi-max-frequency", &temp);
	if (rc < 0)
		spi->max_speed_hz = CM710X_SPI_FREQ;
	else
		spi->max_speed_hz = temp;
}

static int cm710x_SPI_probe(struct spi_device *spi)
{
	dev_info(&spi->dev, "++++++ %s entry_point\n", __func__);

	g_spi = spi;
	if (g_spi == NULL)
		dev_err(&spi->dev, "%s get SPI device error\n", __func__);

	cm710x_parse_dt(g_spi);

	dev_info(&spi->dev, "++++++ %s leave_point\n", __func__);

	return 0;
}

static struct spi_driver cm710x_spi_driver = {
	.driver = {
		.name = "cm710x_spi",
		.of_match_table = cm710x_of_match,
	},
	.probe = cm710x_SPI_probe,
};
module_spi_driver(cm710x_spi_driver);

MODULE_DESCRIPTION("ASoC CM710x SPI driver");
MODULE_AUTHOR("Tzung-Dar Tsai <tdtsai@cmedia.com.tw>");
MODULE_LICENSE("GPL v2");
