/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024 The Linux Foundation. All rights reserved.
 */

#ifndef __GPIO_LOW_VOLT_H
#define __GPIO_LOW_VOLT_H

int low_volt_register_callback(void (*callback)(void *data, bool is_throttle), void *data);

#endif //__GPIO_LOW_VOLT_H
