/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
