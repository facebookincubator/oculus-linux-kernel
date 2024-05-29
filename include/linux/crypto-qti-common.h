/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CRYPTO_QTI_COMMON_H
#define _CRYPTO_QTI_COMMON_H

#include <linux/blk-crypto.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/delay.h>

#define RAW_SECRET_SIZE 32
#define QTI_ICE_MAX_BIST_CHECK_COUNT 100
#define QTI_ICE_TYPE_NAME_LEN 8

/* Storage types for crypto */
#define UFS_CE 10
#define SDCC_CE 20

struct ice_mmio_data {
	void __iomem *ice_base_mmio;
#if IS_ENABLED(CONFIG_QTI_HW_KEY_MANAGER)
	void __iomem *ice_hwkm_mmio;
#endif
};

#if IS_ENABLED(CONFIG_QTI_CRYPTO_COMMON)
int crypto_qti_init_crypto(void *mmio_data);
int crypto_qti_enable(void *mmio_data);
void crypto_qti_disable(void);
int crypto_qti_debug(const struct ice_mmio_data *mmio_data);
int crypto_qti_keyslot_program(const struct ice_mmio_data *mmio_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot, u8 data_unit_mask,
			       int capid);
int crypto_qti_keyslot_evict(const struct ice_mmio_data *mmio_data,
							unsigned int slot);
int crypto_qti_derive_raw_secret(const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size);

#else
static inline int crypto_qti_init_crypto(void *mmio_data)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_enable(void *mmio_data)
{
	return -EOPNOTSUPP;
}
static inline void crypto_qti_disable(void)
{
	return;
}
static inline int crypto_qti_debug(void)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_program(
						const struct ice_mmio_data *mmio_data,
					    const struct blk_crypto_key *key,
					    unsigned int slot,
					    u8 data_unit_mask,
					    int capid)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_keyslot_evict(const struct ice_mmio_data *mmio_data,
						unsigned int slot)
{
	return -EOPNOTSUPP;
}
static inline int crypto_qti_derive_raw_secret(
					    const u8 *wrapped_key,
					    unsigned int wrapped_key_size,
					    u8 *secret,
					    unsigned int secret_size)
{
	return -EOPNOTSUPP;
}

#endif /* CONFIG_QTI_CRYPTO_COMMON */

#endif /* _CRYPTO_QTI_COMMON_H */
