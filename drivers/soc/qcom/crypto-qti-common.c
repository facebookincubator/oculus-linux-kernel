// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/crypto-qti-common.h>
#include "crypto-qti-ice-regs.h"
#include "crypto-qti-platform.h"

static int ice_check_fuse_setting(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;
	uint32_t major, minor;

	major = (ice_entry->ice_hw_version & ICE_CORE_MAJOR_REV_MASK) >>
			ICE_CORE_MAJOR_REV;
	minor = (ice_entry->ice_hw_version & ICE_CORE_MINOR_REV_MASK) >>
			ICE_CORE_MINOR_REV;

	//Check fuse setting is not supported on ICE 3.2 onwards
	if ((major == 0x03) && (minor >= 0x02))
		return 0;
	regval = ice_readl(ice_entry, ICE_REGS_FUSE_SETTING);
	regval &= (ICE_FUSE_SETTING_MASK |
		ICE_FORCE_HW_KEY0_SETTING_MASK |
		ICE_FORCE_HW_KEY1_SETTING_MASK);

	if (regval) {
		pr_err("%s: error: ICE_ERROR_HW_DISABLE_FUSE_BLOWN\n",
				__func__);
		return -EPERM;
	}
	return 0;
}

static int ice_check_version(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t version, major, minor, step;

	version = ice_readl(ice_entry, ICE_REGS_VERSION);
	major = (version & ICE_CORE_MAJOR_REV_MASK) >> ICE_CORE_MAJOR_REV;
	minor = (version & ICE_CORE_MINOR_REV_MASK) >> ICE_CORE_MINOR_REV;
	step = (version & ICE_CORE_STEP_REV_MASK) >> ICE_CORE_STEP_REV;

	if (major < ICE_CORE_CURRENT_MAJOR_VERSION) {
		pr_err("%s: Unknown ICE device at %lu, rev %d.%d.%d\n",
			__func__, (unsigned long)ice_entry->icemmio_base,
				major, minor, step);
		return -ENODEV;
	}

	ice_entry->ice_hw_version = version;

	return 0;
}

int crypto_qti_init_crypto(struct device *dev, void __iomem *mmio_base,
			   void **priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = devm_kzalloc(dev,
		sizeof(struct crypto_vops_qti_entry),
		GFP_KERNEL);
	if (!ice_entry)
		return -ENOMEM;

	ice_entry->icemmio_base = mmio_base;
	ice_entry->flags = 0;

	err = ice_check_version(ice_entry);
	if (err) {
		pr_err("%s: check version failed, err %d\n", __func__, err);
		return err;
	}

	err = ice_check_fuse_setting(ice_entry);
	if (err)
		return err;

	*priv_data = (void *)ice_entry;

	return err;
}

static void ice_low_power_and_optimization_enable(
		struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_ADVANCED_CONTROL);
	/* Enable low power mode sequence
	 * [0]-0,[1]-0,[2]-0,[3]-7,[4]-0,[5]-0,[6]-0,[7]-0,
	 * Enable CONFIG_CLK_GATING, STREAM2_CLK_GATING and STREAM1_CLK_GATING
	 */
	regval |= 0x7000;
	/* Optimization enable sequence
	 */
	regval |= 0xD807100;
	ice_writel(ice_entry, regval, ICE_REGS_ADVANCED_CONTROL);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

static int ice_wait_bist_status(struct crypto_vops_qti_entry *ice_entry)
{
	int count;
	uint32_t regval;

	for (count = 0; count < QTI_ICE_MAX_BIST_CHECK_COUNT; count++) {
		regval = ice_readl(ice_entry, ICE_REGS_BIST_STATUS);
		if (!(regval & ICE_BIST_STATUS_MASK))
			break;
		udelay(50);
	}

	if (regval) {
		pr_err("%s: wait bist status failed, reg %d\n",
				__func__, regval);
		return -ETIMEDOUT;
	}

	return 0;
}

static void ice_enable_intr(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK);
	regval &= ~ICE_REGS_NON_SEC_IRQ_MASK;
	ice_writel(ice_entry, regval, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

static void ice_disable_intr(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval;

	regval = ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK);
	regval |= ICE_REGS_NON_SEC_IRQ_MASK;
	ice_writel(ice_entry, regval, ICE_REGS_NON_SEC_IRQ_MASK);
	/*
	 * Memory barrier - to ensure write completion before next transaction
	 */
	wmb();
}

int crypto_qti_enable(void *priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	ice_low_power_and_optimization_enable(ice_entry);
	err = ice_wait_bist_status(ice_entry);
	if (err)
		return err;
	ice_enable_intr(ice_entry);

	return err;
}

void crypto_qti_disable(void *priv_data)
{
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return;
	}

	crypto_qti_disable_platform(ice_entry);
	ice_disable_intr(ice_entry);
}

int crypto_qti_resume(void *priv_data)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err = ice_wait_bist_status(ice_entry);

	return err;
}

static void ice_dump_test_bus(struct crypto_vops_qti_entry *ice_entry)
{
	uint32_t regval = 0x1;
	uint32_t val;
	uint8_t bus_selector;
	uint8_t stream_selector;

	pr_err("ICE TEST BUS DUMP:\n");

	for (bus_selector = 0; bus_selector <= 0xF;  bus_selector++) {
		regval = 0x1;	/* enable test bus */
		regval |= bus_selector << 28;
		if (bus_selector == 0xD)
			continue;
		ice_writel(ice_entry, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_entry, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}

	pr_err("ICE TEST BUS DUMP (ICE_STREAM1_DATAPATH_TEST_BUS):\n");
	for (stream_selector = 0; stream_selector <= 0xF; stream_selector++) {
		regval = 0xD0000001;	/* enable stream test bus */
		regval |= stream_selector << 16;
		ice_writel(ice_entry, regval, ICE_REGS_TEST_BUS_CONTROL);
		/*
		 * make sure test bus selector is written before reading
		 * the test bus register
		 */
		wmb();
		val = ice_readl(ice_entry, ICE_REGS_TEST_BUS_REG);
		pr_err("ICE_TEST_BUS_CONTROL: 0x%08x | ICE_TEST_BUS_REG: 0x%08x\n",
			regval, val);
	}
}


int crypto_qti_debug(void *priv_data)
{
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	pr_err("%s: ICE Control: 0x%08x | ICE Reset: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_CONTROL),
		ice_readl(ice_entry, ICE_REGS_RESET));

	pr_err("%s: ICE Version: 0x%08x | ICE FUSE:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_VERSION),
		ice_readl(ice_entry, ICE_REGS_FUSE_SETTING));

	pr_err("%s: ICE Param1: 0x%08x | ICE Param2:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_1),
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_2));

	pr_err("%s: ICE Param3: 0x%08x | ICE Param4:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_3),
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_4));

	pr_err("%s: ICE Param5: 0x%08x | ICE IRQ STTS:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_PARAMETERS_5),
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_STTS));

	pr_err("%s: ICE IRQ MASK: 0x%08x | ICE IRQ CLR:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_MASK),
		ice_readl(ice_entry, ICE_REGS_NON_SEC_IRQ_CLR));

	pr_err("%s: ICE INVALID CCFG ERR STTS: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_INVALID_CCFG_ERR_STTS));

	pr_err("%s: ICE BIST Sts: 0x%08x | ICE Bypass Sts:  0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_BIST_STATUS),
		ice_readl(ice_entry, ICE_REGS_BYPASS_STATUS));

	pr_err("%s: ICE ADV CTRL: 0x%08x | ICE ENDIAN SWAP:	0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_ADVANCED_CONTROL),
		ice_readl(ice_entry, ICE_REGS_ENDIAN_SWAP));

	pr_err("%s: ICE_STM1_ERR_SYND1: 0x%08x | ICE_STM1_ERR_SYND2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_ERROR_SYNDROME1),
		ice_readl(ice_entry, ICE_REGS_STREAM1_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM2_ERR_SYND1: 0x%08x | ICE_STM2_ERR_SYND2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_ERROR_SYNDROME1),
		ice_readl(ice_entry, ICE_REGS_STREAM2_ERROR_SYNDROME2));

	pr_err("%s: ICE_STM1_COUNTER1: 0x%08x | ICE_STM1_COUNTER2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS1),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS2));

	pr_err("%s: ICE_STM1_COUNTER3: 0x%08x | ICE_STM1_COUNTER4: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS3),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS4));

	pr_err("%s: ICE_STM2_COUNTER1: 0x%08x | ICE_STM2_COUNTER2: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS1),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS2));

	pr_err("%s: ICE_STM2_COUNTER3: 0x%08x | ICE_STM2_COUNTER4: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS3),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS4));

	pr_err("%s: ICE_STM1_CTR5_MSB: 0x%08x | ICE_STM1_CTR5_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS5_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS5_LSB));

	pr_err("%s: ICE_STM1_CTR6_MSB: 0x%08x | ICE_STM1_CTR6_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS6_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS6_LSB));

	pr_err("%s: ICE_STM1_CTR7_MSB: 0x%08x | ICE_STM1_CTR7_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS7_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS7_LSB));

	pr_err("%s: ICE_STM1_CTR8_MSB: 0x%08x | ICE_STM1_CTR8_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS8_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS8_LSB));

	pr_err("%s: ICE_STM1_CTR9_MSB: 0x%08x | ICE_STM1_CTR9_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS9_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM1_COUNTERS9_LSB));

	pr_err("%s: ICE_STM2_CTR5_MSB: 0x%08x | ICE_STM2_CTR5_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS5_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS5_LSB));

	pr_err("%s: ICE_STM2_CTR6_MSB: 0x%08x | ICE_STM2_CTR6_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS6_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS6_LSB));

	pr_err("%s: ICE_STM2_CTR7_MSB: 0x%08x | ICE_STM2_CTR7_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS7_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS7_LSB));

	pr_err("%s: ICE_STM2_CTR8_MSB: 0x%08x | ICE_STM2_CTR8_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS8_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS8_LSB));

	pr_err("%s: ICE_STM2_CTR9_MSB: 0x%08x | ICE_STM2_CTR9_LSB: 0x%08x\n",
		ice_entry->ice_dev_type,
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS9_MSB),
		ice_readl(ice_entry, ICE_REGS_STREAM2_COUNTERS9_LSB));

	ice_dump_test_bus(ice_entry);

	return 0;
}

int crypto_qti_keyslot_program(void *priv_data,
			       const struct blk_crypto_key *key,
			       unsigned int slot,
			       u8 data_unit_mask, int capid)
{
	int err1 = 0, err2 = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err1 = crypto_qti_program_key(ice_entry, key, slot,
				data_unit_mask, capid);
	if (err1) {
		pr_err("%s: program key failed with error %d\n",
			__func__, err1);
		err2 = crypto_qti_invalidate_key(ice_entry, slot);
		if (err2) {
			pr_err("%s: invalidate key failed with error %d\n",
				__func__, err2);
		}
	}

	return err1;
}

int crypto_qti_keyslot_evict(void *priv_data, unsigned int slot)
{
	int err = 0;
	struct crypto_vops_qti_entry *ice_entry;

	ice_entry = (struct crypto_vops_qti_entry *) priv_data;
	if (!ice_entry) {
		pr_err("%s: vops ice data is invalid\n", __func__);
		return -EINVAL;
	}

	err = crypto_qti_invalidate_key(ice_entry, slot);
	if (err) {
		pr_err("%s: invalidate key failed with error %d\n",
			__func__, err);
		return err;
	}

	return err;
}

int crypto_qti_derive_raw_secret(const u8 *wrapped_key,
				 unsigned int wrapped_key_size, u8 *secret,
				 unsigned int secret_size)
{
	int err = 0;

	if (wrapped_key_size <= RAW_SECRET_SIZE) {
		pr_err("%s: Invalid wrapped_key_size: %u\n",
				__func__, wrapped_key_size);
		err = -EINVAL;
		return err;
	}
	if (secret_size != RAW_SECRET_SIZE) {
		pr_err("%s: Invalid secret size: %u\n", __func__, secret_size);
		err = -EINVAL;
		return err;
	}

	if (wrapped_key_size > 64)
		err = crypto_qti_tz_raw_secret(wrapped_key, wrapped_key_size,
					       secret, secret_size);
	else
		memcpy(secret, wrapped_key, secret_size);

	return err;
}
