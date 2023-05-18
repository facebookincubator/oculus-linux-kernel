/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _UFSHCD_CRYPTO_QTI_H
#define _UFSHCD_CRYPTO_QTI_H

#include "ufshcd.h"
#include "ufshcd-crypto.h"

void ufshcd_crypto_qti_enable(struct ufs_hba *hba);

void ufshcd_crypto_qti_disable(struct ufs_hba *hba);

int ufshcd_crypto_qti_init_crypto(struct ufs_hba *hba,
	const struct keyslot_mgmt_ll_ops *ksm_ops);

void ufshcd_crypto_qti_setup_rq_keyslot_manager(struct ufs_hba *hba,
					    struct request_queue *q);

void ufshcd_crypto_qti_destroy_rq_keyslot_manager(struct ufs_hba *hba,
			struct request_queue *q);

int ufshcd_crypto_qti_prepare_lrbp_crypto(struct ufs_hba *hba,
			struct scsi_cmnd *cmd, struct ufshcd_lrb *lrbp);

int ufshcd_crypto_qti_complete_lrbp_crypto(struct ufs_hba *hba,
				struct scsi_cmnd *cmd, struct ufshcd_lrb *lrbp);

int ufshcd_crypto_qti_debug(struct ufs_hba *hba);

int ufshcd_crypto_qti_suspend(struct ufs_hba *hba, enum ufs_pm_op pm_op);

int ufshcd_crypto_qti_resume(struct ufs_hba *hba, enum ufs_pm_op pm_op);

int ufshcd_crypto_qti_prep_lrbp_crypto(struct ufs_hba *hba,
				       struct scsi_cmnd *cmd,
				       struct ufshcd_lrb *lrbp);
#ifdef CONFIG_SCSI_UFS_CRYPTO_QTI
void ufshcd_crypto_qti_set_vops(struct ufs_hba *hba);
#else
static inline void ufshcd_crypto_qti_set_vops(struct ufs_hba *hba)
{}
#endif /* CONFIG_SCSI_UFS_CRYPTO_QTI */
#endif /* _UFSHCD_CRYPTO_QTI_H */
