// SPDX-License-Identifier: GPL-2.0-only
/*
 * Crypto TZ library for storage encryption.
 *
 * Copyright (c) 2020-2021, Linux Foundation. All rights reserved.
 */

#include <asm/cacheflush.h>
#include <linux/qcom_scm.h>
#include <linux/qtee_shmbridge.h>
#include <linux/crypto-qti-common.h>
#include <linux/module.h>
#include <linux/of.h>
#include "crypto-qti-platform.h"

#define ICE_CIPHER_MODE_XTS_256 3
#define UFS_CE 10
#define SDCC_CE 20
#define UFS_CARD_CE 30

static bool is_boot_dev_type_emmc(void)
{
	struct device_node *np;
	const char *bootparams;

	np = of_find_node_by_path("/chosen");
	of_property_read_string(np, "bootargs", &bootparams);
	if (!bootparams)
		pr_err("%s: failed to get bootargs property\n", __func__);
	else if (strnstr(bootparams, "androidboot.bootdevice",
			strlen(bootparams)) &&
			strnstr(bootparams, "sdhci", strlen(bootparams)))
		return true;

	return false;

}

int crypto_qti_program_key(const struct ice_mmio_data *mmio_data,
			   const struct blk_crypto_key *key, unsigned int slot,
			   unsigned int data_unit_mask, int capid)
{
	int err = 0;
	struct qtee_shm shm;

	err = qtee_shmbridge_allocate_shm(key->size, &shm);
	if (err)
		return -ENOMEM;

	memcpy(shm.vaddr, key->raw, key->size);
	qtee_shmbridge_flush_shm_buf(&shm);

	if (is_boot_dev_type_emmc())
		err = qcom_scm_config_set_ice_key(slot, shm.paddr, key->size,
						ICE_CIPHER_MODE_XTS_256,
						data_unit_mask, SDCC_CE);
	else
		err = qcom_scm_config_set_ice_key(slot, shm.paddr, key->size,
						ICE_CIPHER_MODE_XTS_256,
						data_unit_mask, UFS_CE);
	if (err)
		pr_err("%s:SCM call Error: 0x%x slot %d\n",
				__func__, err, slot);

	qtee_shmbridge_inv_shm_buf(&shm);
	qtee_shmbridge_free_shm(&shm);

	return err;
}
EXPORT_SYMBOL(crypto_qti_program_key);

int crypto_qti_invalidate_key(const struct ice_mmio_data *mmio_data,
			      unsigned int slot)
{
	int err = 0;

	if (is_boot_dev_type_emmc())
		err = qcom_scm_clear_ice_key(slot, SDCC_CE);
	else
		err = qcom_scm_clear_ice_key(slot, UFS_CE);

	if (err)
		pr_err("%s:SCM call Error: 0x%x\n", __func__, err);

	return err;
}
EXPORT_SYMBOL(crypto_qti_invalidate_key);

int crypto_qti_derive_raw_secret_platform(
				const u8 *wrapped_key,
				unsigned int wrapped_key_size, u8 *secret,
				unsigned int secret_size)
{
	int err = 0;
	struct qtee_shm shm_key, shm_secret;

	err = qtee_shmbridge_allocate_shm(wrapped_key_size, &shm_key);
	if (err)
		return -ENOMEM;

	err = qtee_shmbridge_allocate_shm(secret_size, &shm_secret);
	if (err)
		return -ENOMEM;

	memcpy(shm_key.vaddr, wrapped_key, wrapped_key_size);
	qtee_shmbridge_flush_shm_buf(&shm_key);

	memset(shm_secret.vaddr, 0, secret_size);
	qtee_shmbridge_flush_shm_buf(&shm_secret);

	err = qcom_scm_derive_raw_secret(shm_key.paddr, wrapped_key_size,
					shm_secret.paddr, secret_size);
	if (err) {
		pr_err("%s:SCM call Error for derive raw secret: 0x%x\n",
				__func__, err);
	}

	qtee_shmbridge_inv_shm_buf(&shm_secret);
	memcpy(secret, shm_secret.vaddr, secret_size);

	qtee_shmbridge_inv_shm_buf(&shm_key);
	qtee_shmbridge_free_shm(&shm_key);
	qtee_shmbridge_free_shm(&shm_secret);
	return err;
}
EXPORT_SYMBOL(crypto_qti_derive_raw_secret_platform);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Crypto TZ library for storage encryption");
