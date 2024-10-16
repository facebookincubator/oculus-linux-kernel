/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * TEE driver for goodix fingerprint sensor
 * Copyright (C) 2016 Goodix
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __GF_COMMON_H__
#define __GF_COMMON_H__

#include "gf_spi.h"
/****************Function prototypes*****************/
int  gf_spi_read_bytes(struct gf_dev *gf_dev, unsigned short addr,
		unsigned short data_len, unsigned char *rx_buf);

int  gf_spi_write_bytes(struct gf_dev *gf_dev, unsigned short addr,
		unsigned short data_len, unsigned char *tx_buf);

int  gf_spi_read_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short *value);

int  gf_spi_write_word(struct gf_dev *gf_dev, unsigned short addr, unsigned short value);

int  gf_spi_read_data(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_read_data_bigendian(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_write_data(struct gf_dev *gf_dev, unsigned short addr,
		int len, unsigned char *value);

int  gf_spi_send_cmd(struct gf_dev *gf_dev, unsigned char *cmd, int len);
#endif //__GF_COMMON_H__

