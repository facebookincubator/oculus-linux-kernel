/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __QCOM_QCOM_IO_PGTABLE_H
#define __QCOM_QCOM_IO_PGTABLE_H

#include <linux/io-pgtable.h>

struct qcom_iommu_pgtable_ops {
	void *(*alloc)(void *cookie, gfp_t gfp_mask, int order);
	void (*free)(void *cookie, void *virt, int order, bool deferred_free);
	void (*tlb_add_inv)(void *cookie);
	void (*tlb_sync)(void *cookie);
	void (*log_new_table)(void *cookie, void *virt, unsigned long iova, size_t granule);
	void (*log_remove_table)(void *cookie, void *virt, unsigned long iova, size_t granule);
};

struct qcom_io_pgtable_info {
	struct io_pgtable_cfg cfg;
	const struct qcom_iommu_pgtable_ops *iommu_pgtbl_ops;
	dma_addr_t iova_base;
	dma_addr_t iova_end;
};

#define to_qcom_io_pgtable_info(x)\
container_of((x), struct qcom_io_pgtable_info, cfg)

#define IO_PGTABLE_QUIRK_QCOM_USE_UPSTREAM_HINT BIT(31)
#define IO_PGTABLE_QUIRK_QCOM_USE_LLC_NWA       BIT(30)

#define ARM_V8L_FAST ((unsigned int)-1)
#define QCOM_ARM_64_LPAE_S1 ((unsigned int)-2)

struct io_pgtable_ops *qcom_alloc_io_pgtable_ops(enum io_pgtable_fmt fmt,
				struct qcom_io_pgtable_info *pgtbl_info,
				void *cookie);
void qcom_free_io_pgtable_ops(struct io_pgtable_ops *ops);
void *qcom_io_pgtable_alloc_pages(const struct qcom_iommu_pgtable_ops *ops,
				  struct io_pgtable_cfg *cfg,
				  void *cookie, gfp_t gfp, int order);
void qcom_io_pgtable_free_pages(const struct qcom_iommu_pgtable_ops *ops,
				void *cookie, void *virt, int order,
				bool deferred_free);
static inline void
qcom_io_pgtable_tlb_add_inv(const struct qcom_iommu_pgtable_ops *ops, void *cookie)
{
	ops->tlb_add_inv(cookie);
}

static inline void
qcom_io_pgtable_tlb_sync(const struct qcom_iommu_pgtable_ops *ops, void *cookie)
{
	ops->tlb_sync(cookie);
}

static inline void
qcom_io_pgtable_log_new_table(const struct qcom_iommu_pgtable_ops *ops, void *cookie, void *virt,
				unsigned long iova, size_t granule)
{
	ops->log_new_table(cookie, virt, iova, granule);
}

static inline void
qcom_io_pgtable_log_remove_table(const struct qcom_iommu_pgtable_ops *ops, void *cookie, void *virt,
				unsigned long iova, size_t granule)
{
	ops->log_remove_table(cookie, virt, iova, granule);
}

#ifdef CONFIG_IOMMU_IO_PGTABLE_FAST
extern struct io_pgtable_init_fns io_pgtable_av8l_fast_init_fns;
#endif
#ifdef CONFIG_IOMMU_IO_PGTABLE_LPAE
extern struct io_pgtable_init_fns qcom_io_pgtable_arm_64_lpae_s1_init_fns;
#endif

#endif /* __QCOM_QCOM_IO_PGTABLE_H */
