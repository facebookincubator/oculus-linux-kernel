/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021 Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 */

#ifndef __KGSL_LAZY_H
#define __KGSL_LAZY_H

#include "kgsl.h"

struct kgsl_iommu_context;

#if IS_ENABLED(CONFIG_QCOM_KGSL_LAZY_ALLOCATION)
bool kgsl_lazy_procfs_is_process_lazy(struct kgsl_device *device);
int kgsl_lazy_procfs_process_enable(struct kgsl_device *device, bool enable);

void kgsl_lazy_process_disable(struct kgsl_process_private *private);

void kgsl_lazy_process_list_purge(struct kgsl_device *device);
void kgsl_lazy_process_list_free(struct kgsl_device *device);

void kgsl_memdesc_set_lazy_configuration(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, uint64_t *flags, uint32_t *priv);
void kgsl_memdesc_set_lazy_align(struct kgsl_memdesc *memdesc, u64 size);

void kgsl_free_lazy_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list);

int kgsl_lazy_page_pool_init(struct kgsl_device *device);
void kgsl_lazy_page_pool_exit(void);
void kgsl_lazy_page_pool_resume(void);
void kgsl_lazy_page_pool_suspend(void);

int kgsl_fetch_lazy_page(struct kgsl_memdesc *memdesc, uint64_t offset,
		struct page **pagep);

int kgsl_lazy_vmfault(struct kgsl_memdesc *memdesc, struct vm_area_struct *vma,
		struct vm_fault *vmf);

int kgsl_lazy_gpu_fault_handler(struct kgsl_iommu_context *ctx,
		struct kgsl_pagetable *fault_pt, unsigned long addr);
#else
static inline bool kgsl_lazy_procfs_is_process_lazy(struct kgsl_device *device)
{
	return false;
}

static inline int kgsl_lazy_procfs_process_enable(struct kgsl_device *device,
		bool enable)
{
	/* Return an error code on kernels without lazy allocation support. */
	return -EINVAL;
}

static inline void kgsl_lazy_process_disable(struct kgsl_process_private *private)
{
}

static inline void kgsl_lazy_process_list_purge(struct kgsl_device *device)
{
}

static inline void kgsl_lazy_process_list_free(struct kgsl_device *device)
{
}

static inline void kgsl_memdesc_set_lazy_configuration(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, uint64_t *flags, uint32_t *priv)
{
	/* Disable lazy allocation if support is disabled in the kernel. */
	*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;
}

static inline void kgsl_memdesc_set_lazy_align(struct kgsl_memdesc *memdesc, u64 size)
{
}

static inline void kgsl_free_lazy_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
}

static inline int kgsl_lazy_page_pool_init(struct kgsl_device *device)
{
	return 0;
}

static inline void kgsl_lazy_page_pool_exit(void)
{
}

static inline void kgsl_lazy_page_pool_resume(void)
{
}

static inline void kgsl_lazy_page_pool_suspend(void)
{
}

static inline int kgsl_fetch_lazy_page(struct kgsl_memdesc *memdesc, uint64_t offset,
		struct page **pagep)
{
	return -ENOTSUPP;
}

static inline int kgsl_lazy_vmfault(struct kgsl_memdesc *memdesc,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}

static inline int kgsl_lazy_gpu_fault_handler(struct kgsl_iommu_context *ctx,
		struct kgsl_pagetable *fault_pt, unsigned long addr)
{
	return -ENOENT;
}
#endif

#endif /* __KGSL_LAZY_H */
