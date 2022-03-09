/*
 * SPI STP Char Dev header
 *
 * Copyright (C) 2020 Eugen Pirvu
 * Copyright (C) 2020 Facebook, Inc.
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

#ifndef STP_DEV_H
#define STP_DEV_H

#include <linux/kernel.h>

int stp_dev_init(struct device *dev);

int stp_dev_remove(void);

int stp_dev_stp_start(void);

int stp_raw_dev_init(struct spi_device *spi);
int stp_raw_dev_remove(void);

#endif
