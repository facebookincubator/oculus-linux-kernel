/*
 * rt5679-spi.h  --  ALC5679 ALSA SoC audio codec driver
 *
 * Copyright 2015 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CM7120_SPI_H__
#define __CM7120_SPI_H__

#define CM7120_SPI_BUF_LEN 240

/* SPI Command */
enum {
	CM7120_SPI_CMD_16_READ = 0,
	CM7120_SPI_CMD_16_WRITE,
	CM7120_SPI_CMD_32_READ,
	CM7120_SPI_CMD_32_WRITE,
	CM7120_SPI_CMD_BURST_READ,
	CM7120_SPI_CMD_BURST_WRITE,
};

int cm7120_spi_read(u32 addr, unsigned int *val, size_t len);
int cm7120_spi_write(u32 addr, unsigned int val, size_t len);
int cm7120_spi_burst_read(u32 addr, u8 *rxbuf, size_t len);
int cm7120_spi_burst_write(u32 addr, const u8 *txbuf, size_t len);

#endif /* __CM7120_SPI_H__ */
