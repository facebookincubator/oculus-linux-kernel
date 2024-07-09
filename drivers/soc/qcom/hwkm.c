// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI hardware key manager driver.
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <linux/bitops.h>
#include <crypto/hash.h>
#include <crypto/sha.h>
#include <linux/iommu.h>

#include <linux/hwkm.h>
#include "hwkmregs.h"
#include "hwkm_serialize.h"

#define BYTES_TO_WORDS(bytes) (((bytes) + 3) / 4)

#define WRITE_TO_KDF_PACKET(cmd_ptr, src, len)	\
	do {					\
		memcpy(cmd_ptr, src, len);	\
		cmd_ptr += len;			\
	} while (0)

#define ASYNC_CMD_HANDLING false

// Maximum number of times to poll
#define MAX_RETRIES 20000

int retries;
#define WAIT_UNTIL(cond)			\
for (retries = 0; !(cond) && (retries < MAX_RETRIES); retries++)

#define ICEMEM_SLAVE_TPKEY_VAL	0x192
#define ICEMEM_SLAVE_TPKEY_SLOT	0x92
#define KM_MASTER_TPKEY_SLOT	10

struct hwkm_clk_info {
	struct list_head list;
	struct clk *clk;
	const char *name;
	u32 max_freq;
	u32 min_freq;
	u32 curr_freq;
	bool enabled;
};

struct hwkm_device {
	struct device *dev;
	void __iomem *km_base;
	void __iomem *ice_base;
	struct resource *km_res;
	struct resource *ice_res;
	struct list_head clk_list_head;
	bool is_hwkm_clk_available;
	bool is_hwkm_enabled;
};

static struct hwkm_device *km_device;

#define qti_hwkm_readl(hwkm, reg, dest)				\
	(((dest) == KM_MASTER) ?				\
	(readl_relaxed((hwkm)->km_base + (reg))) :		\
	(readl_relaxed((hwkm)->ice_base + (reg))))
#define qti_hwkm_writel(hwkm, val, reg, dest)			\
	(((dest) == KM_MASTER) ?				\
	(writel_relaxed((val), (hwkm)->km_base + (reg))) :	\
	(writel_relaxed((val), (hwkm)->ice_base + (reg))))
#define qti_hwkm_setb(hwkm, reg, nr, dest) {			\
	u32 val = qti_hwkm_readl(hwkm, reg, dest);		\
	val |= (0x1 << nr);					\
	qti_hwkm_writel(hwkm, val, reg, dest);			\
}
#define qti_hwkm_clearb(hwkm, reg, nr, dest) {			\
	u32 val = qti_hwkm_readl(hwkm, reg, dest);		\
	val &= ~(0x1 << nr);					\
	qti_hwkm_writel(hwkm, val, reg, dest);			\
}

static inline bool qti_hwkm_testb(struct hwkm_device *hwkm, u32 reg, u8 nr,
				  enum hwkm_destination dest)
{
	u32 val = qti_hwkm_readl(hwkm, reg, dest);

	val = (val >> nr) & 0x1;
	if (val == 0)
		return false;
	return true;
}

static inline unsigned int qti_hwkm_get_reg_data(struct hwkm_device *dev,
						 u32 reg, u32 offset, u32 mask,
						 enum hwkm_destination dest)
{
	u32 val = 0;

	val = qti_hwkm_readl(dev, reg, dest);
	return ((val & mask) >> offset);
}

/**
 * @brief Send a command packet to the HWKM Master instance as described
 *        in section 3.2.5.1 of Key Manager HPG
 *        - Clear CMD FIFO
 *        - Clear Error Status Register
 *        - Write CMD_ENABLE = 1
 *        - for word in cmd_packet:
 *          - poll until CMD_FIFO_AVAILABLE_SPACE > 0.
 *            Timeout error after 1,000 retries.
 *          - write word to CMD register
 *        - for word in rsp_packet:
 *          - poll until RSP_FIFO_AVAILABLE_DATA > 0.
 *            Timeout error after 1,000 retries.
 *          - read word from RSP register
 *        - Verify CMD_DONE == 1
 *        - Clear CMD_DONE
 *
 * @return HWKM_SUCCESS if successful. HWKW Error Code otherwise.
 */

static int qti_hwkm_master_transaction(struct hwkm_device *dev,
				       const uint32_t *cmd_packet,
				       size_t cmd_words,
				       uint32_t *rsp_packet,
				       size_t rsp_words)
{
	int i = 0;
	int err = 0;

	// Clear CMD FIFO
	qti_hwkm_setb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, KM_MASTER);
	/* Write memory barrier */
	wmb();
	qti_hwkm_clearb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, KM_MASTER);
	/* Write memory barrier */
	wmb();

	// Clear previous CMD errors
	qti_hwkm_writel(dev, 0x0, QTI_HWKM_MASTER_RG_BANK2_BANKN_ESR,
			KM_MASTER);
	/* Write memory barrier */
	wmb();

	// Enable command
	qti_hwkm_setb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_CTL, CMD_ENABLE_BIT,
			KM_MASTER);
	/* Write memory barrier */
	wmb();

	if (qti_hwkm_testb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, KM_MASTER)) {

		pr_err("%s: CMD_FIFO_CLEAR_BIT not set\n", __func__);
		err = -1;
		return -err;
	}

	for (i = 0; i < cmd_words; i++) {
		WAIT_UNTIL(qti_hwkm_get_reg_data(dev,
			QTI_HWKM_MASTER_RG_BANK2_BANKN_STATUS,
			CMD_FIFO_AVAILABLE_SPACE, CMD_FIFO_AVAILABLE_SPACE_MASK,
			KM_MASTER) > 0);
		if (qti_hwkm_get_reg_data(dev,
			QTI_HWKM_MASTER_RG_BANK2_BANKN_STATUS,
			CMD_FIFO_AVAILABLE_SPACE, CMD_FIFO_AVAILABLE_SPACE_MASK,
			KM_MASTER) == 0) {
			pr_err("%s: cmd fifo space not available\n", __func__);
			err = -1;
			return err;
		}
		qti_hwkm_writel(dev, cmd_packet[i],
				QTI_HWKM_MASTER_RG_BANK2_CMD_0, KM_MASTER);
		/* Write memory barrier */
		wmb();
	}

	for (i = 0; i < rsp_words; i++) {
		WAIT_UNTIL(qti_hwkm_get_reg_data(dev,
			QTI_HWKM_MASTER_RG_BANK2_BANKN_STATUS,
			RSP_FIFO_AVAILABLE_DATA, RSP_FIFO_AVAILABLE_DATA_MASK,
			KM_MASTER) > 0);
		if (qti_hwkm_get_reg_data(dev,
			QTI_HWKM_MASTER_RG_BANK2_BANKN_STATUS,
			RSP_FIFO_AVAILABLE_DATA, RSP_FIFO_AVAILABLE_DATA_MASK,
			KM_MASTER) == 0) {
			pr_err("%s: rsp fifo data not available\n", __func__);
			err = -1;
			return err;
		}
		rsp_packet[i] = qti_hwkm_readl(dev,
				QTI_HWKM_MASTER_RG_BANK2_RSP_0, KM_MASTER);
	}

	if (!qti_hwkm_testb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_IRQ_STATUS,
			CMD_DONE_BIT, KM_MASTER)) {
		pr_err("%s: CMD_DONE_BIT not set\n", __func__);
		err = -1;
		return err;
	}

	// Clear CMD_DONE status bit
	qti_hwkm_setb(dev, QTI_HWKM_MASTER_RG_BANK2_BANKN_IRQ_STATUS,
			CMD_DONE_BIT, KM_MASTER);
	/* Write memory barrier */
	wmb();

	return err;
}

/**
 * @brief Send a command packet to the HWKM ICE slave instance as described in
 *        section 3.2.5.1 of Key Manager HPG
 *        - Clear CMD FIFO
 *        - Clear Error Status Register
 *        - Write CMD_ENABLE = 1
 *        - for word in cmd_packet:
 *          - poll until CMD_FIFO_AVAILABLE_SPACE > 0.
 *            Timeout error after 1,000 retries.
 *          - write word to CMD register
 *        - for word in rsp_packet:
 *          - poll until RSP_FIFO_AVAILABLE_DATA > 0.
 *            Timeout error after 1,000 retries.
 *          - read word from RSP register
 *        - Verify CMD_DONE == 1
 *        - Clear CMD_DONE
 *
 * @return HWKM_SUCCESS if successful. HWKW Error Code otherwise.
 */

static int qti_hwkm_ice_transaction(struct hwkm_device *dev,
				    const uint32_t *cmd_packet,
				    size_t cmd_words,
				    uint32_t *rsp_packet,
				    size_t rsp_words)
{
	int i = 0;
	int err = 0;

	// Clear CMD FIFO
	qti_hwkm_setb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();
	qti_hwkm_clearb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	// Clear previous CMD errors
	qti_hwkm_writel(dev, 0x0, QTI_HWKM_ICE_RG_BANK0_BANKN_ESR,
			ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	// Enable command
	qti_hwkm_setb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_CTL, CMD_ENABLE_BIT,
			ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	if (qti_hwkm_testb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_CTL,
			CMD_FIFO_CLEAR_BIT, ICEMEM_SLAVE)) {

		pr_err("%s: CMD_FIFO_CLEAR_BIT not set\n", __func__);
		err = -1;
		return err;
	}

	for (i = 0; i < cmd_words; i++) {
		WAIT_UNTIL(qti_hwkm_get_reg_data(dev,
			QTI_HWKM_ICE_RG_BANK0_BANKN_STATUS,
			CMD_FIFO_AVAILABLE_SPACE, CMD_FIFO_AVAILABLE_SPACE_MASK,
			ICEMEM_SLAVE) > 0);
		if (qti_hwkm_get_reg_data(dev,
			QTI_HWKM_ICE_RG_BANK0_BANKN_STATUS,
			CMD_FIFO_AVAILABLE_SPACE, CMD_FIFO_AVAILABLE_SPACE_MASK,
			ICEMEM_SLAVE) == 0) {
			pr_err("%s: cmd fifo space not available\n", __func__);
			err = -1;
			return err;
		}
		qti_hwkm_writel(dev, cmd_packet[i],
				QTI_HWKM_ICE_RG_BANK0_CMD_0, ICEMEM_SLAVE);
		/* Write memory barrier */
		wmb();
	}

	for (i = 0; i < rsp_words; i++) {
		WAIT_UNTIL(qti_hwkm_get_reg_data(dev,
			QTI_HWKM_ICE_RG_BANK0_BANKN_STATUS,
			RSP_FIFO_AVAILABLE_DATA, RSP_FIFO_AVAILABLE_DATA_MASK,
			ICEMEM_SLAVE) > 0);
		if (qti_hwkm_get_reg_data(dev,
			QTI_HWKM_ICE_RG_BANK0_BANKN_STATUS,
			RSP_FIFO_AVAILABLE_DATA, RSP_FIFO_AVAILABLE_DATA_MASK,
			ICEMEM_SLAVE) == 0) {
			pr_err("%s: rsp fifo data not available\n", __func__);
			err = -1;
			return err;
		}
		rsp_packet[i] = qti_hwkm_readl(dev,
				QTI_HWKM_ICE_RG_BANK0_RSP_0, ICEMEM_SLAVE);
	}

	if (!qti_hwkm_testb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_IRQ_STATUS,
			CMD_DONE_BIT, ICEMEM_SLAVE)) {
		pr_err("%s: CMD_DONE_BIT not set\n", __func__);
		err = -1;
		return err;
	}

	// Clear CMD_DONE status bit
	qti_hwkm_setb(dev, QTI_HWKM_ICE_RG_BANK0_BANKN_IRQ_STATUS,
			CMD_DONE_BIT, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	return err;
}

/*
 * @brief Send a command packet to the selected KM instance and read
 *        the response
 *
 * @param dest            [in]  Destination KM instance
 * @param cmd_packet      [in]  pointer to start of command packet
 * @param cmd_words       [in]  words in the command packet
 * @param rsp_packet      [out] pointer to start of response packet
 * @param rsp_words       [in]  words in the response buffer
 *
 * @return HWKM_SUCCESS if successful. HWKW Error Code otherwise.
 */

static int qti_hwkm_run_transaction(enum hwkm_destination dest,
				    const uint32_t *cmd_packet,
				    size_t cmd_words,
				    uint32_t *rsp_packet,
				    size_t rsp_words)
{
	int status = 0;

	if (cmd_packet == NULL || rsp_packet == NULL) {
		status = -1;
		return status;
	}

	switch (dest) {
	case KM_MASTER:
		status = qti_hwkm_master_transaction(km_device,
					cmd_packet, cmd_words,
					rsp_packet, rsp_words);
		break;
	case ICEMEM_SLAVE:
		status = qti_hwkm_ice_transaction(km_device,
					cmd_packet, cmd_words,
					rsp_packet, rsp_words);
		break;
	default:
		status = -2;
		break;
	}

	return status;
}

static void serialize_policy(struct hwkm_serialized_policy *out,
			     const struct hwkm_key_policy *policy)
{
	memset(out, 0, sizeof(struct hwkm_serialized_policy));
	out->wrap_with_tpkey = policy->wrap_with_tpk_allowed;
	out->hw_destination = policy->hw_destination;
	out->security_level = policy->security_lvl;
	out->swap_export_allowed = policy->swap_export_allowed;
	out->wrap_export_allowed = policy->wrap_export_allowed;
	out->key_type = policy->key_type;
	out->kdf_depth = policy->kdf_depth;
	out->encrypt_allowed = policy->enc_allowed;
	out->decrypt_allowed = policy->dec_allowed;
	out->alg_allowed = policy->alg_allowed;
	out->key_management_by_tz_secure_allowed = policy->km_by_tz_allowed;
	out->key_management_by_nonsecure_allowed = policy->km_by_nsec_allowed;
	out->key_management_by_modem_allowed = policy->km_by_modem_allowed;
	out->key_management_by_spu_allowed = policy->km_by_spu_allowed;
}

static void serialize_kdf_bsve(struct hwkm_kdf_bsve *out,
			       const struct hwkm_bsve *bsve, u8 mks)
{
	memset(out, 0, sizeof(struct hwkm_kdf_bsve));
	out->mks = mks;
	out->key_policy_version_en = bsve->km_key_policy_ver_en;
	out->apps_secure_en = bsve->km_apps_secure_en;
	out->msa_secure_en = bsve->km_msa_secure_en;
	out->lcm_fuse_row_en = bsve->km_lcm_fuse_en;
	out->boot_stage_otp_en = bsve->km_boot_stage_otp_en;
	out->swc_en = bsve->km_swc_en;
	out->fuse_region_sha_digest_en = bsve->km_fuse_region_sha_digest_en;
	out->child_key_policy_en = bsve->km_child_key_policy_en;
	out->mks_en = bsve->km_mks_en;
}

static void deserialize_policy(struct hwkm_key_policy *out,
			       const struct hwkm_serialized_policy *policy)
{
	memset(out, 0, sizeof(struct hwkm_key_policy));
	out->wrap_with_tpk_allowed = policy->wrap_with_tpkey;
	out->hw_destination = policy->hw_destination;
	out->security_lvl = policy->security_level;
	out->swap_export_allowed = policy->swap_export_allowed;
	out->wrap_export_allowed = policy->wrap_export_allowed;
	out->key_type = policy->key_type;
	out->kdf_depth = policy->kdf_depth;
	out->enc_allowed = policy->encrypt_allowed;
	out->dec_allowed = policy->decrypt_allowed;
	out->alg_allowed = policy->alg_allowed;
	out->km_by_tz_allowed = policy->key_management_by_tz_secure_allowed;
	out->km_by_nsec_allowed = policy->key_management_by_nonsecure_allowed;
	out->km_by_modem_allowed = policy->key_management_by_modem_allowed;
	out->km_by_spu_allowed = policy->key_management_by_spu_allowed;
}

static void reverse_key(u8 *key, size_t keylen)
{
	size_t left = 0;
	size_t right = 0;

	for (left = 0, right = keylen - 1; left < right; left++, right--) {
		key[left] ^= key[right];
		key[right] ^= key[left];
		key[left] ^= key[right];
	}
}

/*
 * Command packet format (word indices):
 * CMD[0]    = Operation info (OP, IRQ_EN, DKS, LEN)
 * CMD[1:17] = Wrapped Key Blob
 * CMD[18]   = CRC (disabled)
 *
 * Response packet format (word indices):
 * RSP[0]    = Operation info (OP, IRQ_EN, LEN)
 * RSP[1]    = Error status
 */

static int qti_handle_key_unwrap_import(const struct hwkm_cmd *cmd_in,
					struct hwkm_rsp *rsp_in)
{
	int status = 0;
	u32 cmd[UNWRAP_IMPORT_CMD_WORDS] = {0};
	u32 rsp[UNWRAP_IMPORT_RSP_WORDS] = {0};
	struct hwkm_operation_info operation = {
		.op = KEY_UNWRAP_IMPORT,
		.irq_en = ASYNC_CMD_HANDLING,
		.slot1_desc = cmd_in->unwrap.dks,
		.slot2_desc = cmd_in->unwrap.kwk,
		.len = UNWRAP_IMPORT_CMD_WORDS
	};

	pr_debug("%s: KEY_UNWRAP_IMPORT start\n", __func__);

	memcpy(cmd, &operation, OPERATION_INFO_LENGTH);
	memcpy(cmd + COMMAND_WRAPPED_KEY_IDX, cmd_in->unwrap.wkb,
			cmd_in->unwrap.sz);

	status = qti_hwkm_run_transaction(ICEMEM_SLAVE, cmd,
			UNWRAP_IMPORT_CMD_WORDS, rsp, UNWRAP_IMPORT_RSP_WORDS);
	if (status) {
		pr_err("%s: Error running transaction %d\n", __func__, status);
		return status;
	}

	rsp_in->status = rsp[RESPONSE_ERR_IDX];
	if (rsp_in->status) {
		pr_err("%s: KEY_UNWRAP_IMPORT error status 0x%x\n", __func__,
								rsp_in->status);
		return rsp_in->status;
	}

	return status;
}

/*
 * Command packet format (word indices):
 * CMD[0] = Operation info (OP, IRQ_EN, DKS, DK, LEN)
 * CMD[1] = CRC (disabled)
 *
 * Response packet format (word indices):
 * RSP[0] = Operation info (OP, IRQ_EN, LEN)
 * RSP[1] = Error status
 */

static int qti_handle_keyslot_clear(const struct hwkm_cmd *cmd_in,
				    struct hwkm_rsp *rsp_in)
{
	int status = 0;
	u32 cmd[KEYSLOT_CLEAR_CMD_WORDS] = {0};
	u32 rsp[KEYSLOT_CLEAR_RSP_WORDS] = {0};
	struct hwkm_operation_info operation = {
		.op = KEY_SLOT_CLEAR,
		.irq_en = ASYNC_CMD_HANDLING,
		.slot1_desc = cmd_in->clear.dks,
		.op_flag = cmd_in->clear.is_double_key,
		.len = KEYSLOT_CLEAR_CMD_WORDS
	};

	pr_debug("%s: KEY_SLOT_CLEAR start\n", __func__);

	memcpy(cmd, &operation, OPERATION_INFO_LENGTH);

	status = qti_hwkm_run_transaction(ICEMEM_SLAVE, cmd,
				KEYSLOT_CLEAR_CMD_WORDS, rsp,
				KEYSLOT_CLEAR_RSP_WORDS);
	if (status) {
		pr_err("%s: Error running transaction %d\n", __func__, status);
		return status;
	}

	rsp_in->status = rsp[RESPONSE_ERR_IDX];
	if (rsp_in->status) {
		pr_debug("%s: KEYSLOT_CLEAR error status 0x%x\n",
				__func__, rsp_in->status);
		return rsp_in->status;
	}

	return status;
}

/*
 * NOTE: The command packet can vary in length. If BE = 0, the last 2 indices
 * for the BSVE are skipped. Similarly, if Software Context Length (SCL) < 16,
 * only SCL words are written to the packet. The CRC word is after the last
 * word of the SWC. The LEN field of this command does not include the SCL
 * (unlike other commands where the LEN field is the length of the entire
 * packet). The HW will expect SCL + LEN words to be sent.
 *
 * Command packet format (word indices):
 * CMD[0]    = Operation info (OP, IRQ_EN, DKS, KDK, BE, SCL, LEN)
 * CMD[1:2]  = Policy
 * CMD[3]    = BSVE[0] if BE = 1, 0 if BE = 0
 * CMD[4:5]  = BSVE[1:2] if BE = 1, skipped if BE = 0
 * CMD[6:21] = Software Context, only writing the number of words in SCL
 * CMD[22]   = CRC
 *
 * Response packet format (word indices):
 * RSP[0]    = Operation info (OP, IRQ_EN, LEN)
 * RSP[1]    = Error status
 */

static int qti_handle_system_kdf(const struct hwkm_cmd *cmd_in,
				 struct hwkm_rsp *rsp_in)
{
	int status = 0;
	u32 cmd[SYSTEM_KDF_CMD_MAX_WORDS] = {0};
	u32 rsp[SYSTEM_KDF_RSP_WORDS] = {0};
	u8 *cmd_ptr = (u8 *) cmd;
	struct hwkm_serialized_policy policy;
	struct hwkm_operation_info operation = {
		.op = SYSTEM_KDF,
		.irq_en = ASYNC_CMD_HANDLING,
		.slot1_desc = cmd_in->kdf.dks,
		.slot2_desc = cmd_in->kdf.kdk,
		.op_flag = cmd_in->kdf.bsve.enabled,
		.context_len = BYTES_TO_WORDS(cmd_in->kdf.sz),
		.len = SYSTEM_KDF_CMD_MIN_WORDS +
			(cmd_in->kdf.bsve.enabled ? BSVE_WORDS : 1)
	};

	pr_debug("%s: SYSTEM_KDF start\n", __func__);

	serialize_policy(&policy, &cmd_in->kdf.policy);

	WRITE_TO_KDF_PACKET(cmd_ptr, &operation, OPERATION_INFO_LENGTH);
	WRITE_TO_KDF_PACKET(cmd_ptr, &policy, KEY_POLICY_LENGTH);

	if (cmd_in->kdf.bsve.enabled) {
		struct hwkm_kdf_bsve bsve;

		serialize_kdf_bsve(&bsve, &cmd_in->kdf.bsve, cmd_in->kdf.mks);
		WRITE_TO_KDF_PACKET(cmd_ptr, &bsve, MAX_BSVE_LENGTH);
	} else {
		// Skip the remaining 3 bytes of the current word
		cmd_ptr += 3 * (sizeof(u8));
	}

	WRITE_TO_KDF_PACKET(cmd_ptr, cmd_in->kdf.ctx, cmd_in->kdf.sz);

	status = qti_hwkm_run_transaction(ICEMEM_SLAVE, cmd,
				operation.len + operation.context_len,
				rsp, SYSTEM_KDF_RSP_WORDS);
	if (status) {
		pr_err("%s: Error running transaction %d\n", __func__, status);
		return status;
	}

	rsp_in->status = rsp[RESPONSE_ERR_IDX];
	if (rsp_in->status) {
		pr_err("%s: SYSTEM_KDF error status 0x%x\n", __func__,
					rsp_in->status);
		return rsp_in->status;
	}

	return status;
}

/*
 * Command packet format (word indices):
 * CMD[0] = Operation info (OP, IRQ_EN, SKS, LEN)
 * CMD[1] = CRC (disabled)
 *
 * Response packet format (word indices):
 * RSP[0] = Operation info (OP, IRQ_EN, LEN)
 * RSP[1] = Error status
 */

static int qti_handle_set_tpkey(const struct hwkm_cmd *cmd_in,
				struct hwkm_rsp *rsp_in)
{
	int status = 0;
	u32 cmd[SET_TPKEY_CMD_WORDS] = {0};
	u32 rsp[SET_TPKEY_RSP_WORDS] = {0};
	struct hwkm_operation_info operation = {
		.op = SET_TPKEY,
		.irq_en = ASYNC_CMD_HANDLING,
		.slot1_desc = cmd_in->set_tpkey.sks,
		.len = SET_TPKEY_CMD_WORDS
	};

	pr_debug("%s: SET_TPKEY start\n", __func__);

	memcpy(cmd, &operation, OPERATION_INFO_LENGTH);

	status = qti_hwkm_run_transaction(KM_MASTER, cmd,
			SET_TPKEY_CMD_WORDS, rsp, SET_TPKEY_RSP_WORDS);
	if (status) {
		pr_err("%s: Error running transaction %d\n", __func__, status);
		return status;
	}

	rsp_in->status = rsp[RESPONSE_ERR_IDX];
	if (rsp_in->status) {
		pr_err("%s: SET_TPKEY error status 0x%x\n", __func__,
					rsp_in->status);
		return rsp_in->status;
	}

	return status;
}

/**
 * 254 * NOTE: To anyone maintaining or porting this code wondering why the key
 * is reversed in the command packet: the plaintext key value is expected by
 * the HW in reverse byte order.
 *       See section 1.8.2.2 of the HWKM CPAS for more details
 *       Mapping of key to CE key read order:
 *       Key[255:224] -> CRYPTO0_CRYPTO_ENCR_KEY0
 *       Key[223:192] -> CRYPTO0_CRYPTO_ENCR_KEY1
 *       ...
 *       Key[63:32]   -> CRYPTO0_CRYPTO_ENCR_KEY6
 *       Key[31:0]    -> CRYPTO0_CRYPTO_ENCR_KEY7
 *       In this notation Key[31:0] is the least significant word of the key
 *       If the key length is less than 256 bits, the key is filled in from
 *       higher index to lower
 *       For example, for a 128 bit key, Key[255:128] would have the key,
 *       Key[127:0] would be all 0
 *       This means that CMD[3:6] is all 0, CMD[7:10] has the key value.
 *
 * Command packet format (word indices):
 * CMD[0]    = Operation info (OP, IRQ_EN, DKS/SKS, WE, LEN)
 * CMD[1:2]  = Policy (0 if we == 0)
 * CMD[3:10] = Write key value (0 if we == 0)
 * CMD[11]   = CRC (disabled)
 *
 * Response packet format (word indices):
 * RSP[0]    = Operation info (OP, IRQ_EN, LEN)
 * RSP[1]    = Error status
 * RSP[2:3]  = Policy (0 if we == 1)
 * RSP[4:11] = Read key value (0 if we == 1)
 **/

static int qti_handle_keyslot_rdwr(const struct hwkm_cmd *cmd_in,
				   struct hwkm_rsp *rsp_in)
{
	int status = 0;
	u32 cmd[KEYSLOT_RDWR_CMD_WORDS] = {0};
	u32 rsp[KEYSLOT_RDWR_RSP_WORDS] = {0};
	struct hwkm_serialized_policy policy;
	struct hwkm_operation_info operation = {
		.op = KEY_SLOT_RDWR,
		.irq_en = ASYNC_CMD_HANDLING,
		.slot1_desc = cmd_in->rdwr.slot,
		.op_flag = cmd_in->rdwr.is_write,
		.len = KEYSLOT_RDWR_CMD_WORDS
	};

	pr_debug("%s: KEY_SLOT_RDWR start\n", __func__);
	memcpy(cmd, &operation, OPERATION_INFO_LENGTH);

	if (cmd_in->rdwr.is_write) {
		serialize_policy(&policy, &cmd_in->rdwr.policy);
		memcpy(cmd + COMMAND_KEY_POLICY_IDX, &policy,
				KEY_POLICY_LENGTH);
		memcpy(cmd + COMMAND_KEY_VALUE_IDX, cmd_in->rdwr.key,
				cmd_in->rdwr.sz);
		// Need to reverse the key because the HW expects it in reverse
		// byte order
		reverse_key((u8 *) (cmd + COMMAND_KEY_VALUE_IDX),
				HWKM_MAX_KEY_SIZE);
	}

	status = qti_hwkm_run_transaction(ICEMEM_SLAVE, cmd,
			KEYSLOT_RDWR_CMD_WORDS, rsp, KEYSLOT_RDWR_RSP_WORDS);
	if (status) {
		pr_err("%s: Error running transaction %d\n", __func__, status);
		return status;
	}

	rsp_in->status = rsp[RESPONSE_ERR_IDX];
	if (rsp_in->status) {
		pr_err("%s: KEY_SLOT_RDWR error status 0x%x\n",
				__func__, rsp_in->status);
		return rsp_in->status;
	}

	if (!cmd_in->rdwr.is_write &&
			(rsp_in->status == 0)) {
		memcpy(&policy, rsp + RESPONSE_KEY_POLICY_IDX,
						KEY_POLICY_LENGTH);
		memcpy(rsp_in->rdwr.key,
			rsp + RESPONSE_KEY_VALUE_IDX, RESPONSE_KEY_LENGTH);
		// Need to reverse the key because the HW returns it in
		// reverse byte order
		reverse_key(rsp_in->rdwr.key, HWKM_MAX_KEY_SIZE);
		deserialize_policy(&rsp_in->rdwr.policy, &policy);
	}

	// Clear cmd and rsp buffers, since they may contain plaintext keys
	memset(cmd, 0, sizeof(cmd));
	memset(rsp, 0, sizeof(rsp));

	return status;
}

static int qti_hwkm_parse_clock_info(struct platform_device *pdev,
				     struct hwkm_device *hwkm_dev)
{
	int ret = -1, cnt, i, len;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	char *name;
	struct hwkm_clk_info *clki;
	u32 *clkfreq = NULL;

	if (!np)
		goto out;

	cnt = of_property_count_strings(np, "clock-names");
	if (cnt <= 0) {
		dev_info(dev, "%s: Unable to find clocks, assuming enabled\n",
				__func__);
		ret = cnt;
		goto out;
	}

	if (!of_get_property(np, "qcom,op-freq-hz", &len)) {
		dev_info(dev, "qcom,op-freq-hz property not specified\n");
		goto out;
	}

	len = len/sizeof(*clkfreq);
	if (len != cnt)
		goto out;

	clkfreq = devm_kzalloc(dev, len * sizeof(*clkfreq), GFP_KERNEL);
	if (!clkfreq) {
		ret = -ENOMEM;
		goto out;
	}
	ret = of_property_read_u32_array(np, "qcom,op-freq-hz", clkfreq, len);

	INIT_LIST_HEAD(&hwkm_dev->clk_list_head);

	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(np,
			"clock-names", i, (const char **)&name);
		if (ret)
			goto out;

		clki = devm_kzalloc(dev, sizeof(*clki), GFP_KERNEL);
		if (!clki) {
			ret = -ENOMEM;
			goto out;
		}
		clki->max_freq = clkfreq[i];
		clki->name = kstrdup(name, GFP_KERNEL);
		list_add_tail(&clki->list, &hwkm_dev->clk_list_head);
	}
out:
	return ret;
}

static int qti_hwkm_init_clocks(struct hwkm_device *hwkm_dev)
{
	int ret = -EINVAL;
	struct hwkm_clk_info *clki = NULL;
	struct device *dev = hwkm_dev->dev;
	struct list_head *head = &hwkm_dev->clk_list_head;

	if (!hwkm_dev->is_hwkm_clk_available)
		return 0;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s: HWKM clock list null/empty\n", __func__);
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		clki->clk = devm_clk_get(dev, clki->name);
		if (IS_ERR(clki->clk)) {
			ret = PTR_ERR(clki->clk);
			dev_err(dev, "%s: %s clk get failed, %d\n",
					__func__, clki->name, ret);
			goto out;
		}

		ret = 0;
		if (clki->max_freq) {
			ret = clk_set_rate(clki->clk, clki->max_freq);
			if (ret) {
				dev_err(dev,
				"%s: %s clk set rate(%dHz) failed, %d\n",
				__func__, clki->name, clki->max_freq, ret);
				goto out;
			}
			clki->curr_freq = clki->max_freq;
			dev_dbg(dev, "%s: clk: %s, rate: %lu\n", __func__,
				clki->name, clk_get_rate(clki->clk));
		}
	}
out:
	return ret;
}

static int qti_hwkm_enable_disable_clocks(struct hwkm_device *hwkm_dev,
					  bool enable)
{
	int ret = 0;
	struct hwkm_clk_info *clki = NULL;
	struct device *dev = hwkm_dev->dev;
	struct list_head *head = &hwkm_dev->clk_list_head;

	if (!head || list_empty(head)) {
		dev_err(dev, "%s: HWKM clock list null/empty\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	if (!hwkm_dev->is_hwkm_clk_available) {
		dev_err(dev, "%s: HWKM clock not available\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(clki, head, list) {
		if (!clki->name)
			continue;

		if (enable)
			ret = clk_prepare_enable(clki->clk);
		else
			clk_disable_unprepare(clki->clk);

		if (ret) {
			dev_err(dev, "Unable to %s HWKM clock\n",
				enable?"enable":"disable");
			goto out;
		}
	}
out:
	return ret;
}

int qti_hwkm_clocks(bool on)
{
	int ret = 0;

	ret = qti_hwkm_enable_disable_clocks(km_device, on);
	if (ret) {
		pr_err("%s:%pK Could not enable/disable clocks\n",
				__func__, km_device);
	}

	return ret;
}
EXPORT_SYMBOL(qti_hwkm_clocks);

static int qti_hwkm_get_device_tree_data(struct platform_device *pdev,
					 struct hwkm_device *hwkm_dev)
{
	struct device *dev = &pdev->dev;
	int ret = 0;

	hwkm_dev->km_res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "km_master");
	hwkm_dev->ice_res = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "ice_slave");
	if (!hwkm_dev->km_res || !hwkm_dev->ice_res) {
		pr_err("%s: No memory available for IORESOURCE\n", __func__);
		return -ENOMEM;
	}

	hwkm_dev->km_base = devm_ioremap_resource(dev, hwkm_dev->km_res);
	hwkm_dev->ice_base = devm_ioremap_resource(dev, hwkm_dev->ice_res);

	if (IS_ERR(hwkm_dev->km_base) || IS_ERR(hwkm_dev->ice_base)) {
		ret = PTR_ERR(hwkm_dev->km_base);
		pr_err("%s: Error = %d mapping HWKM memory\n", __func__, ret);
		goto out;
	}

	hwkm_dev->is_hwkm_clk_available = of_property_read_bool(
				dev->of_node, "qcom,enable-hwkm-clk");

	if (hwkm_dev->is_hwkm_clk_available) {
		ret = qti_hwkm_parse_clock_info(pdev, hwkm_dev);
		if (ret) {
			pr_err("%s: qti_hwkm_parse_clock_info failed (%d)\n",
				__func__, ret);
			goto out;
		}
	}

out:
	return ret;
}

int qti_hwkm_handle_cmd(struct hwkm_cmd *cmd, struct hwkm_rsp *rsp)
{
	switch (cmd->op) {
	case SYSTEM_KDF:
		return qti_handle_system_kdf(cmd, rsp);
	case KEY_UNWRAP_IMPORT:
		return qti_handle_key_unwrap_import(cmd, rsp);
	case KEY_SLOT_CLEAR:
		return qti_handle_keyslot_clear(cmd, rsp);
	case KEY_SLOT_RDWR:
		return qti_handle_keyslot_rdwr(cmd, rsp);
	case SET_TPKEY:
		return qti_handle_set_tpkey(cmd, rsp);
	case NIST_KEYGEN:
	case KEY_WRAP_EXPORT:
	case QFPROM_KEY_RDWR: // cmd for HW initialization cmd only
	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(qti_hwkm_handle_cmd);

static void qti_hwkm_configure_slot_access(struct hwkm_device *dev)
{
	qti_hwkm_writel(dev, 0xffffffff,
		QTI_HWKM_ICE_RG_BANK0_AC_BANKN_BBAC_0, ICEMEM_SLAVE);
	qti_hwkm_writel(dev, 0xffffffff,
		QTI_HWKM_ICE_RG_BANK0_AC_BANKN_BBAC_1, ICEMEM_SLAVE);
	qti_hwkm_writel(dev, 0xffffffff,
		QTI_HWKM_ICE_RG_BANK0_AC_BANKN_BBAC_2, ICEMEM_SLAVE);
	qti_hwkm_writel(dev, 0xffffffff,
		QTI_HWKM_ICE_RG_BANK0_AC_BANKN_BBAC_3, ICEMEM_SLAVE);
	qti_hwkm_writel(dev, 0xffffffff,
		QTI_HWKM_ICE_RG_BANK0_AC_BANKN_BBAC_4, ICEMEM_SLAVE);
}

static int qti_hwkm_check_bist_status(struct hwkm_device *hwkm_dev)
{
	if (!qti_hwkm_testb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_STATUS,
		BIST_DONE, ICEMEM_SLAVE)) {
		pr_err("%s: Error with BIST_DONE\n", __func__);
		return -EINVAL;
	}

	if (!qti_hwkm_testb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_STATUS,
		CRYPTO_LIB_BIST_DONE, ICEMEM_SLAVE)) {
		pr_err("%s: Error with CRYPTO_LIB_BIST_DONE\n", __func__);
		return -EINVAL;
	}

	if (!qti_hwkm_testb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_STATUS,
		BOOT_CMD_LIST1_DONE, ICEMEM_SLAVE)) {
		pr_err("%s: Error with BOOT_CMD_LIST1_DONE\n", __func__);
		return -EINVAL;
	}

	if (!qti_hwkm_testb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_STATUS,
		BOOT_CMD_LIST0_DONE, ICEMEM_SLAVE)) {
		pr_err("%s: Error with BOOT_CMD_LIST0_DONE\n", __func__);
		return -EINVAL;
	}

	if (!qti_hwkm_testb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_STATUS,
		KT_CLEAR_DONE, ICEMEM_SLAVE)) {
		pr_err("%s: KT_CLEAR_DONE\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int qti_hwkm_ice_init_sequence(struct hwkm_device *hwkm_dev)
{
	int ret = 0;

	// Put ICE in standard mode
	qti_hwkm_writel(hwkm_dev, 0x7, QTI_HWKM_ICE_RG_TZ_KM_CTL, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	ret = qti_hwkm_check_bist_status(hwkm_dev);
	if (ret) {
		pr_err("%s: Error in BIST initialization %d\n", __func__, ret);
		return ret;
	}

	// Disable CRC checks
	qti_hwkm_clearb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_KM_CTL, CRC_CHECK_EN,
			ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	// Configure key slots to be accessed by HLOS
	qti_hwkm_configure_slot_access(hwkm_dev);
	/* Write memory barrier */
	wmb();

	// Clear RSP_FIFO_FULL bit
	qti_hwkm_setb(hwkm_dev, QTI_HWKM_ICE_RG_BANK0_BANKN_IRQ_STATUS,
			RSP_FIFO_FULL, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();

	return ret;
}

static void qti_hwkm_enable_slave_receive_mode(struct hwkm_device *hwkm_dev)
{
	qti_hwkm_clearb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_TPKEY_RECEIVE_CTL,
			TPKEY_EN, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();
	qti_hwkm_writel(hwkm_dev, ICEMEM_SLAVE_TPKEY_VAL,
			QTI_HWKM_ICE_RG_TZ_TPKEY_RECEIVE_CTL, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();
}

static void qti_hwkm_disable_slave_receive_mode(struct hwkm_device *hwkm_dev)
{
	qti_hwkm_clearb(hwkm_dev, QTI_HWKM_ICE_RG_TZ_TPKEY_RECEIVE_CTL,
			TPKEY_EN, ICEMEM_SLAVE);
	/* Write memory barrier */
	wmb();
}

static void qti_hwkm_check_tpkey_status(struct hwkm_device *hwkm_dev)
{
	int val = 0;

	val = qti_hwkm_readl(hwkm_dev, QTI_HWKM_ICE_RG_TZ_TPKEY_RECEIVE_STATUS,
			ICEMEM_SLAVE);

	pr_debug("%s: Tpkey receive status 0x%x\n", __func__, val);
}

static int qti_hwkm_set_tpkey(void)
{
	int ret = 0;
	struct hwkm_cmd cmd;
	struct hwkm_rsp rsp;

	cmd.op = SET_TPKEY;
	cmd.set_tpkey.sks = KM_MASTER_TPKEY_SLOT;

	qti_hwkm_enable_slave_receive_mode(km_device);
	ret = qti_hwkm_handle_cmd(&cmd, &rsp);
	if (ret) {
		pr_err("%s: Error running commands\n", __func__, ret);
		return ret;
	}

	qti_hwkm_check_tpkey_status(km_device);
	qti_hwkm_disable_slave_receive_mode(km_device);

	return 0;
}

int qti_hwkm_init(void)
{
	int ret = 0;

	ret = qti_hwkm_ice_init_sequence(km_device);
	if (ret) {
		pr_err("%s: Error in ICE init sequence %d\n", __func__, ret);
		return ret;
	}

	ret = qti_hwkm_set_tpkey();
	if (ret) {
		pr_err("%s: Error setting ICE to receive %d\n", __func__, ret);
		return ret;
	}
	/* Write memory barrier */
	wmb();
	return ret;
}
EXPORT_SYMBOL(qti_hwkm_init);

static int qti_hwkm_probe(struct platform_device *pdev)
{
	struct hwkm_device *hwkm_dev;
	int ret = 0;

	pr_debug("%s %d: HWKM probe start\n", __func__, __LINE__);
	if (!pdev) {
		pr_err("%s: Invalid platform_device passed\n", __func__);
		return -EINVAL;
	}

	hwkm_dev = kzalloc(sizeof(struct hwkm_device), GFP_KERNEL);
	if (!hwkm_dev) {
		ret = -ENOMEM;
		pr_err("%s: Error %d allocating memory for HWKM device\n",
			__func__, ret);
		goto err_hwkm_dev;
	}

	hwkm_dev->dev = &pdev->dev;
	if (!hwkm_dev->dev) {
		ret = -EINVAL;
		pr_err("%s: Invalid device passed in platform_device\n",
			__func__);
		goto err_hwkm_dev;
	}

	if (pdev->dev.of_node)
		ret = qti_hwkm_get_device_tree_data(pdev, hwkm_dev);
	else {
		ret = -EINVAL;
		pr_err("%s: HWKM device node not found\n", __func__);
	}
	if (ret)
		goto err_hwkm_dev;

	ret = qti_hwkm_init_clocks(hwkm_dev);
	if (ret) {
		pr_err("%s: Error initializing clocks %d\n", __func__, ret);
		goto err_hwkm_dev;
	}

	hwkm_dev->is_hwkm_enabled = true;
	km_device = hwkm_dev;
	platform_set_drvdata(pdev, hwkm_dev);

	pr_debug("%s %d: HWKM probe ends\n", __func__, __LINE__);
	return ret;

err_hwkm_dev:
	km_device = NULL;
	kfree(hwkm_dev);
	return ret;
}


static int qti_hwkm_remove(struct platform_device *pdev)
{
	kfree(km_device);
	return 0;
}

static const struct of_device_id qti_hwkm_match[] = {
	{ .compatible = "qcom,hwkm"},
	{},
};
MODULE_DEVICE_TABLE(of, qti_hwkm_match);

static struct platform_driver qti_hwkm_driver = {
	.probe		= qti_hwkm_probe,
	.remove		= qti_hwkm_remove,
	.driver		= {
		.name	= "qti_hwkm",
		.of_match_table	= qti_hwkm_match,
	},
};
module_platform_driver(qti_hwkm_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("QTI Hardware Key Manager driver");
