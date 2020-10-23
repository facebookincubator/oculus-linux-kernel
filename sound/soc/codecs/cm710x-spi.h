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

#ifndef __CM710X__SPI_H__
#define __CM710X_SPI_H__

int cm710x_write_SPI_Dsp(u32 uAddr, u8 *Data, size_t len);
int cm710x_read_SPI_Dsp(u32 uAddr, u8 *Data, size_t len);

#endif /* __CM710X_SPI_H__ */
