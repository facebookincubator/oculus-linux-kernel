/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018,2021, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_TZ_SMMU_H__
#define __MSM_TZ_SMMU_H__

#include <linux/device.h>
#include <linux/iommu.h>

enum tz_smmu_device_id {
	TZ_DEVICE_START = 0,
	TZ_DEVICE_VIDEO = 0,
	TZ_DEVICE_MDSS,
	TZ_DEVICE_LPASS,
	TZ_DEVICE_MDSS_BOOT,
	TZ_DEVICE_USB1_HS,
	TZ_DEVICE_OCMEM,
	TZ_DEVICE_LPASS_CORE,
	TZ_DEVICE_VPU,
	TZ_DEVICE_COPSS_SMMU,
	TZ_DEVICE_USB3_0,
	TZ_DEVICE_USB3_1,
	TZ_DEVICE_PCIE_0,
	TZ_DEVICE_PCIE_1,
	TZ_DEVICE_BCSS,
	TZ_DEVICE_VCAP,
	TZ_DEVICE_PCIE20,
	TZ_DEVICE_IPA,
	TZ_DEVICE_APPS,
	TZ_DEVICE_GPU,
	TZ_DEVICE_UFS,
	TZ_DEVICE_ICE,
	TZ_DEVICE_ROT,
	TZ_DEVICE_VFE,
	TZ_DEVICE_ANOC0,
	TZ_DEVICE_ANOC1,
	TZ_DEVICE_ANOC2,
	TZ_DEVICE_CPP,
	TZ_DEVICE_JPEG,
	TZ_DEVICE_MAX,
};

#ifdef CONFIG_MSM_TZ_SMMU

int msm_tz_smmu_atos_start(struct device *dev, int cb_num);
int msm_tz_smmu_atos_end(struct device *dev, int cb_num);
enum tz_smmu_device_id msm_dev_to_device_id(struct device *dev);
int msm_tz_set_cb_format(enum tz_smmu_device_id sec_id, int cbndx);
int msm_iommu_sec_pgtbl_init(void);
int register_iommu_sec_ptbl(void);
bool arm_smmu_skip_write(void __iomem *addr);
extern void *get_smmu_from_addr(struct iommu_device *iommu, void __iomem *addr);
extern void *arm_smmu_get_by_addr(void __iomem *addr);
/* Donot write to smmu global space with CONFIG_MSM_TZ_SMMU */
#undef writel_relaxed
#undef writeq_relaxed
#define writel_relaxed(v, c)	do {					\
	if (!arm_smmu_skip_write(c))					\
		((void)__raw_writel((u32)cpu_to_le32(v), (c)));	\
	} while (0)

#define writeq_relaxed(v, c) do {                              \
	if (!arm_smmu_skip_write(c))                            \
		((void)__raw_writeq((u64)cpu_to_le64(v), (c))); \
	} while (0)
#else

static inline int msm_tz_smmu_atos_start(struct device *dev, int cb_num)
{
	return 0;
}

static inline int msm_tz_smmu_atos_end(struct device *dev, int cb_num)
{
	return 0;
}

static inline enum tz_smmu_device_id msm_dev_to_device_id(struct device *dev)
{
	return -EINVAL;
}

static inline int msm_tz_set_cb_format(enum tz_smmu_device_id sec_id,
					int cbndx)
{
	return -EINVAL;
}

static inline int msm_iommu_sec_pgtbl_init(void)
{
	return -EINVAL;
}

static inline int register_iommu_sec_ptbl(void)
{
	return -EINVAL;
}

static inline size_t msm_secure_smmu_unmap(struct iommu_domain *domain,
					   unsigned long iova,
					   size_t size)
{
	return -EINVAL;
}

static inline size_t msm_secure_smmu_map_sg(struct iommu_domain *domain,
					    unsigned long iova,
					    struct scatterlist *sg,
					    unsigned int nents, int prot)
{
	return -EINVAL;
}

static inline int msm_secure_smmu_map(struct iommu_domain *domain,
				      unsigned long iova,
				      phys_addr_t paddr, size_t size, int prot)
{
	return -EINVAL;
}

#endif /* CONFIG_MSM_TZ_SMMU */

#endif /* __MSM_TZ_SMMU_H__ */
