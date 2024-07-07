// SPDX-License-Identifier: GPL-2.0-or-later
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

#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/timer.h>
#include <linux/err.h>

#include "gf_spi.h"
#include <linux/spi/spi.h>
#include <linux/spi/spidev.h>

#define PROPERTY_GPIO(np, string, target)				\
	(target = of_get_named_gpio_flags(np, string, 0, NULL))

#define PROPERTY_BOOL(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = 0;					\
		else							\
			target = (u8)tmp_val;				\
	} while (0)

#define PROPERTY_U32(np, string, target)				\
	do {								\
		u32 tmp_val = 0;					\
		if (of_property_read_u32(np, string, &tmp_val) < 0)	\
			target = -1;					\
		else							\
			target = tmp_val;				\
	} while (0)

#define PROPERTY_STRING_ARRAY(np, string, target, cnt)			\
	do {								\
		int i;						\
		cnt = of_property_count_strings(np, string);		\
		for (i = 0; i < cnt; i++) {				\
			of_property_read_string_index(np, string, i,	\
						      &target[i]);	\
		}							\
	} while (0)

#define PROPERTY_STRING(np, string, target) \
	(target = of_property_read_string(np, string, &target))

int gf_parse_dts(struct gf_dev *gf_dev)
{
	int rc = GF_NO_ERROR;
	struct device_node *np = gf_dev->spi->dev.of_node;

	FUNC_ENTRY();

	PROPERTY_GPIO(np, "goodix,gpio_reset", gf_dev->reset_gpio);
	gf_dbg("gf:gpio_reset: %d\n", gf_dev->reset_gpio);
	if (!gpio_is_valid(gf_dev->reset_gpio)) {
		gf_dbg("RESET GPIO is invalid.\n");
		return -GF_PERM_ERROR;
	}
	rc = gpio_request(gf_dev->reset_gpio, "goodix_reset");
	if (rc) {
		dev_err(&gf_dev->spi->dev, "Failed to request RESET GPIO. rc = %d\n", rc);
		return -GF_PERM_ERROR;
	}
	gpio_direction_output(gf_dev->reset_gpio, 0);

	PROPERTY_GPIO(np, "goodix,gpio_irq", gf_dev->irq_gpio);
	if (!gpio_is_valid(gf_dev->irq_gpio)) {
		gf_dbg("IRQ GPIO is invalid.\n");
		return -GF_PERM_ERROR;
	}
	gf_dbg("gf:irq_gpio:%d\n", gf_dev->irq_gpio);

	rc = gpio_request(gf_dev->irq_gpio, "goodix_irq");
	if (rc) {
		dev_err(&gf_dev->spi->dev, "Failed to request IRQ GPIO. rc = %d\n", rc);
		return -GF_PERM_ERROR;
	}
	gpio_direction_input(gf_dev->irq_gpio);

	PROPERTY_GPIO(np, "goodix,vdd-gpio", gf_dev->pwr_gpio);
	if (!gpio_is_valid(gf_dev->pwr_gpio)) {
		gf_dbg("vdd-io GPIO is invalid.\n");
		return -GF_PERM_ERROR;
	}
	rc = gpio_request(gf_dev->pwr_gpio, "goodix_vdd_io");
	if (rc) {
		dev_err(&gf_dev->spi->dev, "Failed to request vdd-io GPIO. rc = %d\n", rc);
		return -GF_PERM_ERROR;
	}
	gpio_direction_output(gf_dev->pwr_gpio, 0);

	FUNC_EXIT();

	return GF_NO_ERROR;
}

void gf_cleanup(struct gf_dev *gf_dev)
{
	gf_dbg("[info] %s\n", __func__);
	if (gpio_is_valid(gf_dev->irq_gpio)) {
		gpio_free(gf_dev->irq_gpio);
		gf_dbg("remove irq_gpio success\n");
	}
	if (gpio_is_valid(gf_dev->reset_gpio)) {
		gpio_free(gf_dev->reset_gpio);
		gf_dbg("remove reset_gpio success\n");
	}
}


int gf_power_on(struct gf_dev *gf_dev)
{
	int rc = 0;

	msleep_interruptible(10);
	gf_dbg("---- power on ok ----\n");

	return rc;
}

int gf_power_off(struct gf_dev *gf_dev)
{
	int rc = 0;

	gf_dbg("---- power off ----\n");
	return rc;
}

/********************************************************************
 *CPU output low level in RST pin to reset GF. This is the MUST action for GF.
 *Take care of this function. IO Pin driver strength / glitch and so on.
 ********************************************************************/
int gf_hw_reset(struct gf_dev *gf_dev, unsigned int delay_ms)
{
	if (gf_dev == NULL) {
		gf_dbg("Input buff is NULL.\n");
		return -EPERM;
	}
	gpio_direction_output(gf_dev->reset_gpio, 1);
	gpio_set_value(gf_dev->reset_gpio, 0);
	gf_dbg("RST pin status: %d", gpio_get_value(gf_dev->reset_gpio));
	mdelay(3);
	gpio_set_value(gf_dev->reset_gpio, 1);
	mdelay(delay_ms);
	return 0;
}

int gf_irq_num(struct gf_dev *gf_dev)
{
	if (gf_dev == NULL) {
		gf_dbg("Input buff is NULL.\n");
		return -EPERM;
	} else {
		return gpio_to_irq(gf_dev->irq_gpio);
	}
}

