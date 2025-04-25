// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/string.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "cam_common_util.h"
#include "cam_debug_util.h"

int cam_common_util_get_string_index(const char **strings,
	uint32_t num_strings, const char *matching_string, uint32_t *index)
{
	int i;

	for (i = 0; i < num_strings; i++) {
		if (strnstr(strings[i], matching_string, strlen(strings[i]))) {
			CAM_DBG(CAM_UTIL, "matched %s : %d\n",
				matching_string, i);
			*index = i;
			return 0;
		}
	}

	return -EINVAL;
}

uint32_t cam_common_util_remove_duplicate_arr(int32_t *arr, uint32_t num)
{
	int i, j;
	uint32_t wr_idx = 1;

	if (!arr) {
		CAM_ERR(CAM_UTIL, "Null input array");
		return 0;
	}

	for (i = 1; i < num; i++) {
		for (j = 0; j < wr_idx ; j++) {
			if (arr[i] == arr[j])
				break;
		}
		if (j == wr_idx)
			arr[wr_idx++] = arr[i];
	}

	return wr_idx;
}

int cam_common_mem_kdup(void **dst,
	void *src, size_t size)
{
	gfp_t flag = GFP_KERNEL;

	if (!src || !dst || !size) {
		CAM_ERR(CAM_UTIL, "Invalid params src: %pK dst: %pK size: %u",
			src, dst, size);
		return -EINVAL;
	}

	if (!in_task())
		flag = GFP_ATOMIC;

	*dst = kvzalloc(size, flag);
	if (!*dst) {
		CAM_ERR(CAM_UTIL, "Failed to allocate memory with size: %u", size);
		return -ENOMEM;
	}

	memcpy(*dst, src, size);
	CAM_DBG(CAM_UTIL, "Allocate and copy memory with size: %u", size);

	return 0;
}
EXPORT_SYMBOL(cam_common_mem_kdup);

void cam_common_mem_free(void *memory)
{
	kvfree(memory);
}
EXPORT_SYMBOL(cam_common_mem_free);
