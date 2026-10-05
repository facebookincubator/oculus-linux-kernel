// SPDX-License-Identifier: GPL+
/*******************************************************************************
 * @file arfw_mem_util.c
 *
 * @brief Implementation of arfw utility functions.
 *
 *******************************************************************************/

#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include "arfw_mem_util.h"

MODULE_LICENSE("GPL");

/**
 * Validate an arfw device id string. It should be a human readable string
 * consisting of [a-zA-Z0-9-] and up to max_len characters in
 * length.
 */
int arfw_mem_util_check_arfw_device_id(const char *device_id, size_t max_len)
{
	int len, temp;

	if (device_id == NULL)
		return -EINVAL;

	len = strnlen(device_id, max_len + 1);
	if (len > max_len)
		return -E2BIG;

	if (len == 0)
		return -EINVAL;

	for (temp = 0; temp < len; temp++) {
		// only allow [a-zA-Z0-9-]
		if (!isalnum(device_id[temp]) && device_id[temp] != '-')
			return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(arfw_mem_util_check_arfw_device_id);

dma_addr_t arfw_mem_util_get_dma_addr(const struct sg_table *sgt, size_t offset)
{
	int i;
	struct scatterlist *sl;

	for_each_sg(sgt->sgl, sl, sgt->nents, i) {
		if (sl->length > offset)
			return sg_dma_address(sl) + offset;

		offset -= sl->length;
	}

	return 0;
}
EXPORT_SYMBOL(arfw_mem_util_get_dma_addr);

uint8_t arfw_mem_util_iomem_read_8(void *base_va, size_t offset)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	return readb((void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_read_8);

uint16_t arfw_mem_util_iomem_read_16(void *base_va, size_t offset)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	return readw((void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_read_16);

uint32_t arfw_mem_util_iomem_read_32(void *base_va, size_t offset)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	return readl((void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_read_32);

uint64_t arfw_mem_util_iomem_read_64(void *base_va, size_t offset)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	return readq((void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_read_64);

void arfw_mem_util_iomem_read_block(void *base_va, size_t offset, void *dst,
				    size_t len)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	BUG_ON(base_va == NULL);
	BUG_ON(dst == NULL);

	memcpy_fromio(dst, (void *)addr, len);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_read_block);

void arfw_mem_util_iomem_write_8(void *base_va, size_t offset, uint8_t value)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	writeb(value, (void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_write_8);

void arfw_mem_util_iomem_write_16(void *base_va, size_t offset, uint16_t value)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	writew(value, (void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_write_16);

void arfw_mem_util_iomem_write_32(void *base_va, size_t offset, uint32_t value)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	writel(value, (void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_write_32);

void arfw_mem_util_iomem_write_64(void *base_va, size_t offset, uint64_t value)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	writeq(value, (void *)addr);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_write_64);

void arfw_mem_util_iomem_write_block(void *base_va, size_t offset,
				     const void *src, size_t len)
{
	uintptr_t addr = ((uintptr_t)base_va) + offset;

	BUG_ON(base_va == NULL);
	BUG_ON(src == NULL);

	memcpy_toio((void *)addr, src, len);
}
EXPORT_SYMBOL(arfw_mem_util_iomem_write_block);

void arfw_mem_region_free(struct kref *refcnt)
{
	struct arfw_queue_mem_region *mem;

	mem = container_of(refcnt, struct arfw_queue_mem_region, refcnt);

	BUG_ON(mem->ptr == NULL);
	if (!is_vmalloc_addr(mem->ptr))
		kfree(mem->ptr);
	else
		vfree(mem->ptr);

	mem->ptr = NULL;
	mem->size = 0;

	kfree(mem);
}
EXPORT_SYMBOL(arfw_mem_region_free);
