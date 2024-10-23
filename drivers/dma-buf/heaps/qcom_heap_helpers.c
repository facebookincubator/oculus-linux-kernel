// SPDX-License-Identifier: GPL-2.0
/*
 * Orignally copied from drivers/dma-buf/heaps/heap-helpers.c as of commit
 * 5248eb12fea8 ("dma-buf: heaps: Add heap helpers")
 *
 * Addition that skips unneeded syncs in the dma_buf ops taken from
 * drivers/dma-buf/heaps/heap-helpers.c in:
 * https://lore.kernel.org/lkml/20201017013255.43568-5-john.stultz@linaro.org/
 *
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/highmem.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <uapi/linux/dma-heap.h>
#include <linux/qcom_dma_heap.h>

#include "qcom_dma_heap_priv.h"

static const struct dma_buf_ops qcom_heap_helper_ops;

void qcom_init_heap_helper_buffer(struct heap_helper_buffer *buffer,
			     void (*free)(struct heap_helper_buffer *))
{
	buffer->priv_virt = NULL;
	mutex_init(&buffer->lock);
	buffer->vmap_cnt = 0;
	buffer->vaddr = NULL;
	buffer->pagecount = 0;
	buffer->pages = NULL;
	INIT_LIST_HEAD(&buffer->attachments);
	buffer->free = free;
}

struct dma_buf *qcom_heap_helper_export_dmabuf(struct heap_helper_buffer *buffer,
					  int fd_flags)
{
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);

	exp_info.ops = &qcom_heap_helper_ops;
	exp_info.size = buffer->size;
	exp_info.flags = fd_flags;
	exp_info.priv = buffer;

	return dma_buf_export(&exp_info);
}

static void *dma_heap_map_kernel(struct heap_helper_buffer *buffer)
{
	void *vaddr;

	vaddr = vmap(buffer->pages, buffer->pagecount, VM_MAP, PAGE_KERNEL);
	if (!vaddr)
		return ERR_PTR(-ENOMEM);

	return vaddr;
}

static void dma_heap_buffer_destroy(struct heap_helper_buffer *buffer)
{
	if (buffer->vmap_cnt > 0) {
		WARN(1, "%s: buffer still mapped in the kernel\n", __func__);
		vunmap(buffer->vaddr);
	}

	buffer->free(buffer);
}

static void *dma_heap_buffer_vmap_get(struct heap_helper_buffer *buffer)
{
	void *vaddr;

	if (buffer->vmap_cnt) {
		buffer->vmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = dma_heap_map_kernel(buffer);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->vmap_cnt++;
	return vaddr;
}

static void dma_heap_buffer_vmap_put(struct heap_helper_buffer *buffer)
{
	if (!--buffer->vmap_cnt) {
		vunmap(buffer->vaddr);
		buffer->vaddr = NULL;
	}
}

struct dma_heaps_attachment {
	struct device *dev;
	struct sg_table table;
	struct list_head list;
	bool mapped;
};

static int dma_heap_attach(struct dma_buf *dmabuf,
			   struct dma_buf_attachment *attachment)
{
	struct dma_heaps_attachment *a;
	struct heap_helper_buffer *buffer = dmabuf->priv;
	int ret;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	ret = sg_alloc_table_from_pages(&a->table, buffer->pages,
					buffer->pagecount, 0,
					buffer->pagecount << PAGE_SHIFT,
					GFP_KERNEL);
	if (ret) {
		kfree(a);
		return ret;
	}

	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void dma_heap_detach(struct dma_buf *dmabuf,
			    struct dma_buf_attachment *attachment)
{
	struct dma_heaps_attachment *a = attachment->priv;
	struct heap_helper_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);

	sg_free_table(&a->table);
	kfree(a);
}

static
struct sg_table *dma_heap_map_dma_buf(struct dma_buf_attachment *attachment,
				      enum dma_data_direction direction)
{
	struct dma_heaps_attachment *a = attachment->priv;
	struct sg_table *table;

	table = &a->table;

	if (!dma_map_sg(attachment->dev, table->sgl, table->nents,
			direction))
		table = ERR_PTR(-ENOMEM);

	a->mapped = true;
	return table;
}

static void dma_heap_unmap_dma_buf(struct dma_buf_attachment *attachment,
				   struct sg_table *table,
				   enum dma_data_direction direction)
{
	struct dma_heaps_attachment *a = attachment->priv;

	a->mapped = false;
	dma_unmap_sg(attachment->dev, table->sgl, table->nents, direction);
}

static vm_fault_t dma_heap_vm_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct heap_helper_buffer *buffer = vma->vm_private_data;

	if (vmf->pgoff > buffer->pagecount)
		return VM_FAULT_SIGBUS;

	vmf->page = buffer->pages[vmf->pgoff];
	get_page(vmf->page);

	return 0;
}

static const struct vm_operations_struct dma_heap_vm_ops = {
	.fault = dma_heap_vm_fault,
};

static int dma_heap_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;

	if ((vma->vm_flags & (VM_SHARED | VM_MAYSHARE)) == 0)
		return -EINVAL;

	vma->vm_ops = &dma_heap_vm_ops;
	vma->vm_private_data = buffer;

	return 0;
}

static void dma_heap_dma_buf_release(struct dma_buf *dmabuf)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;

	dma_heap_buffer_destroy(buffer);
}

static int dma_heap_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					     enum dma_data_direction direction)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;
	struct dma_heaps_attachment *a;
	int ret = 0;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		invalidate_kernel_vmap_range(buffer->vaddr, buffer->size);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_cpu(a->dev, a->table.sgl, a->table.nents,
				    direction);
	}
	mutex_unlock(&buffer->lock);

	return ret;
}

static int dma_heap_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
					   enum dma_data_direction direction)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;
	struct dma_heaps_attachment *a;

	mutex_lock(&buffer->lock);

	if (buffer->vmap_cnt)
		flush_kernel_vmap_range(buffer->vaddr, buffer->size);

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_device(a->dev, a->table.sgl, a->table.nents,
				       direction);
	}
	mutex_unlock(&buffer->lock);

	return 0;
}

static void *dma_heap_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;
	void *vaddr;

	mutex_lock(&buffer->lock);
	vaddr = dma_heap_buffer_vmap_get(buffer);
	mutex_unlock(&buffer->lock);

	return vaddr;
}

static void dma_heap_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct heap_helper_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	dma_heap_buffer_vmap_put(buffer);
	mutex_unlock(&buffer->lock);
}

static const struct dma_buf_ops qcom_heap_helper_ops = {
	.map_dma_buf = dma_heap_map_dma_buf,
	.unmap_dma_buf = dma_heap_unmap_dma_buf,
	.mmap = dma_heap_mmap,
	.release = dma_heap_dma_buf_release,
	.attach = dma_heap_attach,
	.detach = dma_heap_detach,
	.begin_cpu_access = dma_heap_dma_buf_begin_cpu_access,
	.end_cpu_access = dma_heap_dma_buf_end_cpu_access,
	.vmap = dma_heap_dma_buf_vmap,
	.vunmap = dma_heap_dma_buf_vunmap,
};
