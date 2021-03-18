/*
 * Copyright (c) 2017, Oculus VR. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>

static u32 android_serialno_bin = 0;
static int __init parse_serialno(char *str)
{
	int i, pos;
	for (i = 0; i < strlen(str); i++) {
		/* pos tracks from end of string to beginning */
		pos = strlen(str) - i - 1;
		/* essentially multiplies the value by pow(16, i) */
		android_serialno_bin += hex_to_bin(str[pos]) * (0x1 << (i * 4));
	}
	return 0;
}
__setup("androidboot.serialno=", parse_serialno);

u32 get_android_serialno(void) { return android_serialno_bin; }
EXPORT_SYMBOL(get_android_serialno);
