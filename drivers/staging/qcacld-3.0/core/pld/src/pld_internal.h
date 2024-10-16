/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __PLD_COMMON_I_H__
#define __PLD_COMMON_I_H__

#include "pld_common.h"

struct dev_node {
	struct device *dev;
	struct device *ifdev;
	struct list_head list;
	enum pld_bus_type bus_type;
};

struct pld_context {
	struct pld_driver_ops *ops;
	spinlock_t pld_lock;
	struct list_head dev_list;
	uint32_t pld_driver_state;
	uint8_t mode;
};

/**
 * pld_get_global_context() - Get global context of PLD
 *
 * Return: PLD global context
 */
struct pld_context *pld_get_global_context(void);

/**
 * pld_add_dev() - Add dev node to global context
 * @pld_context: PLD global context
 * @dev: device
 * @ifdev: interface device
 * @type: Bus type
 *
 * Return: 0 for success
 *         Non zero failure code for errors
 */
int pld_add_dev(struct pld_context *pld_context,
		struct device *dev, struct device *ifdev,
		enum pld_bus_type type);

/**
 * pld_del_dev() - Delete dev node from global context
 * @pld_context: PLD global context
 * @dev: device
 *
 * Return: void
 */
void pld_del_dev(struct pld_context *pld_context,
		 struct device *dev);

#endif
