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
void kgsl_memdesc_set_lazy_configuration(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, uint64_t *flags, uint32_t *priv);

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
		struct kgsl_pagetable *fault_pt, unsigned long addr,
		int fault_flags);
#else
static void kgsl_memdesc_set_lazy_configuration(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc, uint64_t *flags, uint32_t *priv)
{
	/* Disable lazy allocation if support is disabled in the kernel. */
	*priv &= ~KGSL_MEMDESC_LAZY_ALLOCATION;
}

static void kgsl_free_lazy_pages(struct kgsl_memdesc *memdesc,
		struct list_head *page_list)
{
}

static int kgsl_lazy_page_pool_init(struct kgsl_device *device)
{
	return 0;
}

static void kgsl_lazy_page_pool_exit(void)
{
}

static void kgsl_lazy_page_pool_resume(void)
{
}

static void kgsl_lazy_page_pool_suspend(void)
{
}

static int kgsl_fetch_lazy_page(struct kgsl_memdesc *memdesc, uint64_t offset,
		struct page **pagep)
{
	return ERR_PTR(-ENOTSUPP);
}

static int kgsl_lazy_vmfault(struct kgsl_memdesc *memdesc,
		struct vm_area_struct *vma, struct vm_fault *vmf)
{
	return VM_FAULT_SIGBUS;
}

static int kgsl_lazy_gpu_fault_handler(struct kgsl_iommu_context *ctx,
		struct kgsl_pagetable *fault_pt, unsigned long addr,
		int fault_flags)
{
	return -ENOENT;
}
#endif

#endif /* __KGSL_LAZY_H */
