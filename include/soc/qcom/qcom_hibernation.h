/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SOC_QCOM_HIBERNATION_H__
#define __SOC_QCOM_HIBERNATION_H__

#include <linux/blk_types.h>
#include <linux/blkdev.h>

#define AES256_KEY_SIZE		32
#define IV_SIZE			12
#define IV_WORDS		3

struct qcom_crypto_params {
	uint32_t authslot_count;
	uint32_t iv[IV_WORDS];
} __packed;

struct hib_bio_batch {
	atomic_t		count;
	wait_queue_head_t	wait;
	blk_status_t		error;
	struct blk_plug		plug;
};

extern int get_key_for_hib_exp(void);

#endif /* __SOC_QCOM_HIBERNATION_H__ */
