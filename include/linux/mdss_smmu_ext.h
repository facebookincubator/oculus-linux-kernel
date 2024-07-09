/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2017, 2020, The Linux Foundation. All rights reserved. */

#ifndef MDSS_SMMU_EXT_H
#define MDSS_SMMU_EXT_H

/**
 * struct msm_smmu:interface exposed to the clients which use smmu driver.
 * @dev:	smmu device for attach/dettach
 * @domain:	domain for the context bank.
 * @is_secure: bool variable to check for secure domain.
 * @iommu_ctrl:	iommu ctrl function for enable/disable attach.
 * @secure_session_ctrl: ctrl function for enable/disable session.
 * @wait_for_transition:function to wait till secure transtion is complete.
 * @reg_lock /reg_unlock: Lock to access shared registers.
 */
struct mdss_smmu_intf {
	struct device *dev;
	int domain;
	bool is_secure;
	int (*iommu_ctrl)(int i);
	int (*secure_session_ctrl)(int i);
	int (*wait_for_transition)(int state, int request);
	void (*reg_lock)(void);
	void (*reg_unlock)(void);
	bool (*handoff_pending)(void);
};

typedef void (*msm_smmu_handler_t) (struct mdss_smmu_intf *smmu);

/**
 * mdss_smmu_request_mappings: function to request smmu mappings.
 *		Client driver can request smmu dev via this API.
 *		dev will be returned in the same call context
 *		if probe is not finished then dev will be
 *		returned once it is completed.
 * @callback:	callback function that is called to return smmu
 *		dev
 */
#ifdef CONFIG_FB_MSM_MDSS
int mdss_smmu_request_mappings(msm_smmu_handler_t callback);
#else
static inline int mdss_smmu_request_mappings(msm_smmu_handler_t callback)
{
	return 0;
}
#endif
#endif /* MDSS_SMMU_EXT_H */
