// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "CAM-SMMU %s:%d\n" fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/dma-buf.h>
#include <asm/dma-iommu.h>
#include <asm/cacheflush.h>
#include <linux/dma-direction.h>
#include <linux/of_platform.h>
#include <linux/iommu.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/msm_dma_iommu_mapping.h>
#include <linux/workqueue.h>
#include <linux/sizes.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/secure_buffer.h>
#include <msm_camera_tz_util.h>
#include <linux/ion_kernel.h>
#include "cam_smmu_api.h"

#define SCRATCH_ALLOC_START SZ_128K
#define SCRATCH_ALLOC_END   SZ_256M
#define VA_SPACE_END	    SZ_2G
#define IOMMU_INVALID_DIR -1
#define BYTE_SIZE 8
#define COOKIE_NUM_BYTE 2
#define COOKIE_SIZE (BYTE_SIZE*COOKIE_NUM_BYTE)
#define COOKIE_MASK ((1<<COOKIE_SIZE)-1)
#define HANDLE_INIT (-1)
#define CAM_SMMU_CB_MAX 2
#define CAM_SMMU_SID_MAX 4


#define GET_SMMU_HDL(x, y) (((x) << COOKIE_SIZE) | ((y) & COOKIE_MASK))
#define GET_SMMU_TABLE_IDX(x) (((x) >> COOKIE_SIZE) & COOKIE_MASK)

#define CAMERA_DEVICE_ID 0x16
#define SECURE_SYSCALL_ID 0x18

#ifdef CONFIG_CAM_SMMU_DBG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) pr_debug(fmt, ##args)
#endif

struct cam_smmu_work_payload {
	int idx;
	struct iommu_domain *domain;
	struct device *dev;
	unsigned long iova;
	int flags;
	void *token;
	struct list_head list;
};

enum cam_protection_type {
	CAM_PROT_INVALID,
	CAM_NON_SECURE,
	CAM_SECURE,
	CAM_PROT_MAX,
};

enum cam_iommu_type {
	CAM_SMMU_INVALID,
	CAM_QSMMU,
	CAM_ARM_SMMU,
	CAM_SMMU_MAX,
};

enum cam_smmu_buf_state {
	CAM_SMMU_BUFF_EXIST,
	CAM_SMMU_BUFF_NOT_EXIST
};

enum cam_smmu_init_dir {
	CAM_SMMU_TABLE_INIT,
	CAM_SMMU_TABLE_DEINIT,
};

struct scratch_mapping {
	void *bitmap;
	size_t bits;
	unsigned int order;
	dma_addr_t base;
};

struct cam_context_bank_info {
	struct device *dev;
	struct iommu_domain *domain;
	enum iommu_attr attr;
	dma_addr_t va_start;
	size_t va_len;
	const char *name;
	bool is_secure;
	uint8_t scratch_buf_support;
	struct scratch_mapping scratch_map;
	struct list_head smmu_buf_list;
	struct mutex lock;
	int handle;
	enum cam_smmu_ops_param state;
	client_handler handler[CAM_SMMU_CB_MAX];
	client_reset_handler hw_reset_handler[CAM_SMMU_CB_MAX];
	void *token[CAM_SMMU_CB_MAX];
	int cb_count;
	int ref_cnt;
	int sids[CAM_SMMU_SID_MAX];
};

struct cam_iommu_cb_set {
	struct cam_context_bank_info *cb_info;
	u32 cb_num;
	u32 cb_init_count;
	bool camera_secure_sid;
	struct work_struct smmu_work;
	struct mutex payload_list_lock;
	struct list_head payload_list;
};

static const struct of_device_id msm_cam_smmu_dt_match[] = {
	{ .compatible = "qcom,msm-cam-smmu", },
	{ .compatible = "qcom,msm-cam-smmu-cb", },
	{}
};

struct cam_dma_buff_info {
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	enum dma_data_direction dir;
	int iommu_dir;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int ion_fd;
	size_t len;
	size_t phys_len;
};

struct cam_sec_buff_info {
	struct dma_buf *dmabuf;
	struct dma_buf_attachment *attach;
	struct sg_table *table;
	enum dma_data_direction dir;
	int ref_count;
	dma_addr_t paddr;
	struct list_head list;
	int ion_fd;
	size_t len;
};

static struct cam_iommu_cb_set iommu_cb_set;

static enum dma_data_direction cam_smmu_translate_dir(
	enum cam_smmu_map_dir dir);

static int cam_smmu_check_handle_unique(int hdl);

static int cam_smmu_create_iommu_handle(int idx);

static int cam_smmu_create_add_handle_in_table(char *name,
	int *hdl);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
	int ion_fd);

static struct cam_sec_buff_info *cam_smmu_find_mapping_by_sec_buf_idx(int idx,
	int ion_fd);

static int cam_smmu_init_scratch_map(struct scratch_mapping *scratch_map,
					dma_addr_t base, size_t size,
					int order);

static int cam_smmu_alloc_scratch_va(struct scratch_mapping *mapping,
					size_t size,
					dma_addr_t *iova);

static int cam_smmu_free_scratch_va(struct scratch_mapping *mapping,
					dma_addr_t addr, size_t size);

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_virt_address(int idx,
		dma_addr_t virt_addr);

static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
	enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
	size_t *len_ptr);

static int cam_smmu_alloc_scratch_buffer_add_to_list(int idx,
					      size_t virt_len,
					      size_t phys_len,
					      unsigned int iommu_dir,
					      dma_addr_t *virt_addr);
static int cam_smmu_unmap_buf_and_remove_from_list(
	struct cam_dma_buff_info *mapping_info, int idx);

static int cam_smmu_free_scratch_buffer_remove_from_list(
					struct cam_dma_buff_info *mapping_info,
					int idx);

static void cam_smmu_clean_buffer_list(int idx);

static void cam_smmu_print_list(int idx);

static void cam_smmu_print_table(void);

static int cam_smmu_probe(struct platform_device *pdev);

static void cam_smmu_check_vaddr_in_range(int idx, void *vaddr);

static void cam_smmu_page_fault_work(struct work_struct *work)
{
	int j;
	int idx;
	struct cam_smmu_work_payload *payload;

	mutex_lock(&iommu_cb_set.payload_list_lock);
	payload = list_first_entry(&iommu_cb_set.payload_list,
			struct cam_smmu_work_payload,
			list);
	list_del(&payload->list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	/* Dereference the payload to call the handler */
	idx = payload->idx;
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	cam_smmu_check_vaddr_in_range(idx, (void *)payload->iova);
	for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
		if ((iommu_cb_set.cb_info[idx].handler[j])) {
			iommu_cb_set.cb_info[idx].handler[j](
				payload->domain,
				payload->dev,
				payload->iova,
				payload->flags,
				iommu_cb_set.cb_info[idx].token[j]);
		}
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	kfree(payload);
}

static void cam_smmu_print_list(int idx)
{
	struct cam_dma_buff_info *mapping;

	pr_err("index = %d\n", idx);
	list_for_each_entry(mapping,
		&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		pr_err("ion_fd = %d, paddr= 0x%pK, len = %u\n",
			 mapping->ion_fd, (void *)mapping->paddr,
			 (unsigned int)mapping->len);
	}
}

static void cam_smmu_print_table(void)
{
	int i;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		pr_err("i= %d, handle= %d, name_addr=%pK\n", i,
			   (int)iommu_cb_set.cb_info[i].handle,
			   (void *)iommu_cb_set.cb_info[i].name);
		pr_err("dev = %pK\n", iommu_cb_set.cb_info[i].dev);
	}
}

static void cam_smmu_check_vaddr_in_range(int idx, void *vaddr)
{
	struct cam_dma_buff_info *mapping;
	unsigned long start_addr, end_addr, current_addr;

	current_addr = (unsigned long)vaddr;
	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		start_addr = (unsigned long)mapping->paddr;
		end_addr = (unsigned long)mapping->paddr + mapping->len;

		if (start_addr <= current_addr && current_addr < end_addr) {
			pr_err("Error: va %pK is valid: range:%pK-%pK, fd = %d cb: %s\n",
				vaddr, (void *)start_addr, (void *)end_addr,
				mapping->ion_fd,
				iommu_cb_set.cb_info[idx].name);
			return;
		}
			CDBG("va %pK is not in this range: %pK-%pK, fd = %d\n",
			vaddr, (void *)start_addr, (void *)end_addr,
				mapping->ion_fd);
	}
	if (!strcmp(iommu_cb_set.cb_info[idx].name, "vfe"))
		pr_err_ratelimited("Cannot find vaddr:%pK in SMMU.\n"
			" %s uses invalid virtual address\n",
			vaddr, iommu_cb_set.cb_info[idx].name);
	else
		pr_err("Cannot find vaddr:%pK in SMMU.\n"
			" %s uses invalid virtual address\n",
			vaddr, iommu_cb_set.cb_info[idx].name);
}

void cam_smmu_reg_client_page_fault_handler(int handle,
		client_handler page_fault_handler,
		client_reset_handler hw_reset_handler,
		void *token)
{
	int idx, i = 0;

	if (!token) {
		pr_err("Error: token is NULL\n");
		return;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return;
	}

	if (page_fault_handler) {
		if (iommu_cb_set.cb_info[idx].cb_count == CAM_SMMU_CB_MAX) {
			pr_err("%s Should not regiester more handlers\n",
				iommu_cb_set.cb_info[idx].name);
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			return;
		}
		iommu_cb_set.cb_info[idx].cb_count++;
		for (i = 0; i < iommu_cb_set.cb_info[idx].cb_count; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == NULL) {
				iommu_cb_set.cb_info[idx].token[i] = token;
				iommu_cb_set.cb_info[idx].handler[i] =
					page_fault_handler;
				iommu_cb_set.cb_info[idx].hw_reset_handler[i] =
					hw_reset_handler;
				break;
			}
		}
	} else {
		for (i = 0; i < CAM_SMMU_CB_MAX; i++) {
			if (iommu_cb_set.cb_info[idx].token[i] == token) {
				iommu_cb_set.cb_info[idx].token[i] = NULL;
				iommu_cb_set.cb_info[idx].handler[i] =
					NULL;
				iommu_cb_set.cb_info[idx].cb_count--;
				break;
			}
		}
		if (i == CAM_SMMU_CB_MAX)
			pr_err("Error: hdl %x no matching tokens: %s\n",
				handle, iommu_cb_set.cb_info[idx].name);
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
}

static int cam_smmu_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long iova,
		int flags, void *token)
{
	char *cb_name;
	int idx;
	int j;
	struct cam_smmu_work_payload *payload;

	if (!token) {
		pr_err("Error: token is NULL\n");
		pr_err("Error: domain = %pK, device = %pK\n", domain, dev);
		pr_err("iova = %lX, flags = %d\n", iova, flags);
		return 0;
	}

	cb_name = (char *)token;
	/* check whether it is in the table */
	for (idx = 0; idx < iommu_cb_set.cb_num; idx++) {
		if (!strcmp(iommu_cb_set.cb_info[idx].name, cb_name))
			break;
	}

	if (idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: index is not valid, index = %d, token = %s\n",
			idx, cb_name);
		return 0;
	}

	payload = kzalloc(sizeof(struct cam_smmu_work_payload), GFP_ATOMIC);
	if (!payload)
		return 0;

	payload->domain = domain;
	payload->dev = dev;
	payload->iova = iova;
	payload->flags = flags;
	payload->token = token;
	payload->idx = idx;

	/* trigger hw reset handler */
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
		if ((iommu_cb_set.cb_info[idx].hw_reset_handler[j])) {
			iommu_cb_set.cb_info[idx].hw_reset_handler[j](
			payload->domain,
			payload->dev,
			iommu_cb_set.cb_info[idx].token[j]);
		}
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);

	mutex_lock(&iommu_cb_set.payload_list_lock);
	list_add_tail(&payload->list, &iommu_cb_set.payload_list);
	mutex_unlock(&iommu_cb_set.payload_list_lock);

	schedule_work(&iommu_cb_set.smmu_work);

	return 0;
}

static int cam_smmu_translate_dir_to_iommu_dir(
			enum cam_smmu_map_dir dir)
{
	switch (dir) {
	case CAM_SMMU_MAP_READ:
		return IOMMU_READ;
	case CAM_SMMU_MAP_WRITE:
		return IOMMU_WRITE;
	case CAM_SMMU_MAP_RW:
		return IOMMU_READ|IOMMU_WRITE;
	case CAM_SMMU_MAP_INVALID:
	default:
		pr_err("Error: Direction is invalid. dir = %d\n", dir);
		break;
	}
	return IOMMU_INVALID_DIR;
}

static enum dma_data_direction cam_smmu_translate_dir(
				enum cam_smmu_map_dir dir)
{
	switch (dir) {
	case CAM_SMMU_MAP_READ:
		return DMA_FROM_DEVICE;
	case CAM_SMMU_MAP_WRITE:
		return DMA_TO_DEVICE;
	case CAM_SMMU_MAP_RW:
		return DMA_BIDIRECTIONAL;
	case CAM_SMMU_MAP_INVALID:
	default:
		pr_err("Error: Direction is invalid. dir = %d\n", (int)dir);
		break;
	}
	return DMA_NONE;
}

static void cam_smmu_reset_iommu_table(enum cam_smmu_init_dir ops)
{
	unsigned int i;
	int j = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_cb_set.cb_info[i].handle = HANDLE_INIT;
		INIT_LIST_HEAD(&iommu_cb_set.cb_info[i].smmu_buf_list);
		iommu_cb_set.cb_info[i].state = CAM_SMMU_DETACH;
		iommu_cb_set.cb_info[i].dev = NULL;
		iommu_cb_set.cb_info[i].cb_count = 0;
		iommu_cb_set.cb_info[i].ref_cnt = 0;
		for (j = 0; j < CAM_SMMU_CB_MAX; j++) {
			iommu_cb_set.cb_info[i].token[j] = NULL;
			iommu_cb_set.cb_info[i].handler[j] = NULL;
		}
		for (j = 0; j < CAM_SMMU_SID_MAX; j++)
			iommu_cb_set.cb_info[i].sids[j] = -1;

		if (ops == CAM_SMMU_TABLE_INIT)
			mutex_init(&iommu_cb_set.cb_info[i].lock);
		else
			mutex_destroy(&iommu_cb_set.cb_info[i].lock);
	}
}

static int cam_smmu_check_handle_unique(int hdl)
{
	int i;

	if (hdl == HANDLE_INIT) {
		CDBG("iommu handle is init number. Need to try again\n");
		return 1;
	}

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (iommu_cb_set.cb_info[i].handle == HANDLE_INIT)
			continue;

		if (iommu_cb_set.cb_info[i].handle == hdl) {
			CDBG("iommu handle %d conflicts\n", (int)hdl);
			return 1;
		}
	}
	return 0;
}

/**
 *  use low 2 bytes for handle cookie
 */
static int cam_smmu_create_iommu_handle(int idx)
{
	int rand, hdl = 0;

	get_random_bytes(&rand, COOKIE_NUM_BYTE);
	hdl = GET_SMMU_HDL(idx, rand);
	CDBG("create handle value = %x\n", (int)hdl);
	return hdl;
}

static int cam_smmu_attach_device(int idx)
{
	int rc;
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	struct iommu_domain *domain =
		iommu_cb_set.cb_info[idx].domain;

	/* attach the mapping to device */
	rc = iommu_attach_device(domain, cb->dev);
	if (rc < 0) {
		pr_err("Error: ARM IOMMU attach failed. ret = %d\n", rc);
		return -ENODEV;
	}
	return rc;
}

static int cam_smmu_create_add_handle_in_table(char *name,
					int *hdl)
{
	int i;
	int handle;

	/* create handle and add in the iommu hardware table */
	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		if (!strcmp(iommu_cb_set.cb_info[i].name, name)) {
			mutex_lock(&iommu_cb_set.cb_info[i].lock);
			if (iommu_cb_set.cb_info[i].handle != HANDLE_INIT) {
				pr_err("Error: %s already got handle 0x%x\n",
						name,
						iommu_cb_set.cb_info[i].handle);
				*hdl = iommu_cb_set.cb_info[i].handle;
				iommu_cb_set.cb_info[i].ref_cnt++;
				mutex_unlock(&iommu_cb_set.cb_info[i].lock);
				return -EINVAL;
			}

			/* make sure handle is unique */
			do {
				handle = cam_smmu_create_iommu_handle(i);
			} while (cam_smmu_check_handle_unique(handle));

			/* put handle in the table */
			iommu_cb_set.cb_info[i].handle = handle;
			iommu_cb_set.cb_info[i].cb_count = 0;
			iommu_cb_set.cb_info[i].ref_cnt++;
			*hdl = handle;
			CDBG("%s creates handle 0x%x\n", name, handle);
			mutex_unlock(&iommu_cb_set.cb_info[i].lock);
			return 0;
		}
	}

	/* if i == iommu_cb_set.cb_num */
	pr_err("Error: Cannot find name %s or all handle exist!\n",
			name);
	cam_smmu_print_table();
	return -EINVAL;
}

static int cam_smmu_init_scratch_map(struct scratch_mapping *scratch_map,
					dma_addr_t base, size_t size,
					int order)
{
	unsigned int count = size >> (PAGE_SHIFT + order);
	unsigned int bitmap_size = BITS_TO_LONGS(count) * sizeof(long);
	int err = 0;

	if (!count) {
		err = -EINVAL;
		pr_err("Error: wrong size passed, page count can't be zero\n");
		goto bail;
	}

	scratch_map->bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!scratch_map->bitmap) {
		err = -ENOMEM;
		goto bail;
	}

	scratch_map->base = base;
	scratch_map->bits = BITS_PER_BYTE * bitmap_size;
	scratch_map->order = order;

bail:
	return err;
}

static int cam_smmu_alloc_scratch_va(struct scratch_mapping *mapping,
					size_t size,
					dma_addr_t *iova)
{
	int rc = 0;
	unsigned int order = get_order(size);
	unsigned int align = 0;
	unsigned int count, start;

	count = ((PAGE_ALIGN(size) >> PAGE_SHIFT) +
		 (1 << mapping->order) - 1) >> mapping->order;

	/* Transparently, add a guard page to the total count of pages
	 * to be allocated
	 */
	count++;

	if (order > mapping->order)
		align = (1 << (order - mapping->order)) - 1;

	start = bitmap_find_next_zero_area(mapping->bitmap, mapping->bits, 0,
					   count, align);

	if (start > mapping->bits)
		rc = -ENOMEM;

	bitmap_set(mapping->bitmap, start, count);

	*iova = mapping->base + (start << (mapping->order + PAGE_SHIFT));
	return rc;
}

static int cam_smmu_free_scratch_va(struct scratch_mapping *mapping,
					dma_addr_t addr, size_t size)
{
	unsigned int start = (addr - mapping->base) >>
			     (mapping->order + PAGE_SHIFT);
	unsigned int count = ((size >> PAGE_SHIFT) +
			      (1 << mapping->order) - 1) >> mapping->order;

	if (!addr) {
		pr_err("Error: Invalid address\n");
		return -EINVAL;
	}

	if (start + count > mapping->bits) {
		pr_err("Error: Invalid page bits in scratch map\n");
		return -EINVAL;
	}

	/* Transparently, add a guard page to the total count of pages
	 * to be freed
	 */
	count++;

	bitmap_clear(mapping->bitmap, start, count);

	return 0;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_virt_address(int idx,
		dma_addr_t virt_addr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->paddr == virt_addr) {
			CDBG("Found virtual address %lx\n",
				 (unsigned long)virt_addr);
			return mapping;
		}
	}

	pr_err("Error: Cannot find virtual address %lx by index %d\n",
		(unsigned long)virt_addr, idx);
	return NULL;
}

static struct cam_dma_buff_info *cam_smmu_find_mapping_by_ion_index(int idx,
		int ion_fd)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
			CDBG(" find ion_fd %d\n", ion_fd);
			return mapping;
		}
	}

	pr_err("Error: Cannot find fd %d by index %d\n",
		ion_fd, idx);
	return NULL;
}

static struct cam_sec_buff_info *cam_smmu_find_mapping_by_sec_buf_idx(int idx,
	int ion_fd)
{
	struct cam_sec_buff_info *mapping;

	list_for_each_entry(mapping, &iommu_cb_set.cb_info[idx].smmu_buf_list,
		list) {
		if (mapping->ion_fd == ion_fd) {
			CDBG("[sec_cam] find ion_fd %d\n", ion_fd);
			return mapping;
		}
	}
	pr_err("Error: Cannot find fd %d by index %d\n",
		ion_fd, idx);
	return NULL;
}

static void cam_smmu_clean_buffer_list(int idx)
{
	int ret;
	struct cam_dma_buff_info *mapping_info, *temp;

	list_for_each_entry_safe(mapping_info, temp,
			&iommu_cb_set.cb_info[idx].smmu_buf_list, list) {
		CDBG("Free mapping address %pK, i = %d, fd = %d\n",
			 (void *)mapping_info->paddr, idx,
			mapping_info->ion_fd);

		if (mapping_info->ion_fd == 0xDEADBEEF)
			/* Clean up scratch buffers */
			ret = cam_smmu_free_scratch_buffer_remove_from_list(
							mapping_info, idx);
		else
			/* Clean up regular mapped buffers */
			ret = cam_smmu_unmap_buf_and_remove_from_list(
					mapping_info,
					idx);

		if (ret < 0) {
			pr_err("Buffer delete failed: idx = %d\n", idx);
			pr_err("Buffer delete failed: addr = %lx, fd = %d\n",
					(unsigned long)mapping_info->paddr,
					mapping_info->ion_fd);
			/*
			 * Ignore this error and continue to delete other
			 * buffers in the list
			 */
			continue;
		}
	}
}

static int cam_smmu_attach(int idx)
{
	int ret;

	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_ATTACH) {
		ret = 0;
	} else if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		ret = cam_smmu_attach_device(idx);
		if (ret < 0) {
			pr_err("Error: ATTACH fail\n");
			return -ENODEV;
		}
		iommu_cb_set.cb_info[idx].state = CAM_SMMU_ATTACH;
		ret = 0;
	} else {
		pr_err("Error: Not detach/attach\n");
		ret = -EINVAL;
	}
	return ret;
}

static int cam_smmu_send_syscall_cpp_intf(int vmid, int idx)
{
	int rc = 0;
	struct scm_desc desc = {0};
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	uint32_t *sid_info = NULL;

	sid_info = kzalloc(sizeof(uint32_t) * 1, GFP_KERNEL);
	if (!sid_info)
		return -ENOMEM;


	sid_info[0] = cb->sids[0]; /* CPP SID */

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
	desc.args[0] = CAMERA_DEVICE_ID;
	desc.args[1] = SCM_BUFFER_PHYS(sid_info);
	desc.args[2] = sizeof(uint32_t);
	desc.args[3] = vmid;
	/*
	 * Syscall to hypervisor to switch CPP SID's
	 * between secure and non-secure contexts
	 */
	dmac_flush_range(sid_info, sid_info + 1);
	if (scm_call2(SCM_SIP_FNID(SCM_SVC_MP, SECURE_SYSCALL_ID),
			&desc)){
		pr_err("call to hypervisor failed\n");
		kfree(sid_info);
		return -EINVAL;
	}
	kfree(sid_info);
	return rc;
}

static int cam_smmu_send_syscall_pix_intf(int vmid, int idx)
{
	int rc = 0;
	struct scm_desc desc = {0};
	uint32_t *sid_info = NULL;
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];

	sid_info = kzalloc(sizeof(uint32_t) * 2, GFP_KERNEL);
	if (!sid_info)
		return -ENOMEM;

	sid_info[0] = cb->sids[0]; /* VFE 0 Image SID */
	sid_info[1] = cb->sids[2]; /* VFE 1 Image SID */

	desc.arginfo = SCM_ARGS(4, SCM_VAL, SCM_RW, SCM_VAL, SCM_VAL);
	desc.args[0] = CAMERA_DEVICE_ID;
	desc.args[1] = SCM_BUFFER_PHYS(sid_info);
	desc.args[2] = sizeof(uint32_t) * 2;
	desc.args[3] = vmid;
	/*
	 * Syscall to hypervisor to switch VFE SID's
	 * between secure and non-secure contexts
	 */
	dmac_flush_range(sid_info, sid_info + 2);
	if (scm_call2(SCM_SIP_FNID(SCM_SVC_MP, SECURE_SYSCALL_ID),
			&desc)){
		pr_err("call to hypervisor failed\n");
		kfree(sid_info);
		return -EINVAL;
	}

	kfree(sid_info);
	return rc;
}

static int cam_smmu_detach_device(int idx)
{
	struct cam_context_bank_info *cb = &iommu_cb_set.cb_info[idx];
	struct iommu_domain *domain =
		iommu_cb_set.cb_info[idx].domain;

	if (!list_empty_careful(&iommu_cb_set.cb_info[idx].smmu_buf_list)) {
		pr_err("Client %s buffer list is not clean!\n",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_list(idx);
		cam_smmu_clean_buffer_list(idx);
	}

	/* detach the mapping to device */
	iommu_detach_device(domain, cb->dev);
	iommu_cb_set.cb_info[idx].state = CAM_SMMU_DETACH;
	return 0;
}

static int cam_smmu_attach_sec_cpp(int idx)
{
	int32_t rc = 0;

	/*
	 * When switching to secure, detach CPP NS, do scm call
	 * with CPP SID and no need of attach again, because
	 * all cpp sids are shared in SCM call. so no need of
	 * attach again.
	 */
	if (cam_smmu_send_syscall_cpp_intf(VMID_CP_CAMERA, idx)) {
		pr_err("error: syscall failed\n");
		return -EINVAL;
	}

	rc = msm_camera_tz_set_mode(MSM_CAMERA_TZ_MODE_SECURE,
		MSM_CAMERA_TZ_HW_BLOCK_CPP);
	if (rc != 0) {
		pr_err("secure mode TA notification for cpp unsuccessful, rc %d\n",
			rc);
		/*
		 * Although the TA notification failed, the flow should proceed
		 * without returning an error as at this point cpp had already
		 * entered the secure mode.
		 */
	}
	iommu_cb_set.cb_info[idx].state = CAM_SMMU_ATTACH;
	return 0;
}

static int cam_smmu_detach_sec_cpp(int idx)
{
	int32_t rc = 0;

	rc = msm_camera_tz_set_mode(MSM_CAMERA_TZ_MODE_NON_SECURE,
		MSM_CAMERA_TZ_HW_BLOCK_CPP);
	if (rc != 0) {
		pr_err("secure mode TA notification for cpp unsuccessful, rc %d\n",
			rc);
		/*
		 * Although the TA notification failed, the flow should proceed
		 * without returning an error, as at this point cpp is in secure
		 * mode and should be switched to non-secure regardless
		 */
	}

	iommu_cb_set.cb_info[idx].state = CAM_SMMU_DETACH;

	/*
	 * When exiting secure, do scm call to attach
	 * with CPP SID in NS mode.
	 */
	if (cam_smmu_send_syscall_cpp_intf(VMID_HLOS, idx)) {
		pr_err("error: syscall failed\n");
		return -EINVAL;
	}
	return 0;
}

static int cam_smmu_attach_sec_vfe_ns_stats(int idx)
{
	int32_t rc = 0;

	if (!iommu_cb_set.camera_secure_sid) {
		/*
		 *When switching to secure, for secure pix and non-secure stats
		 *localizing scm/attach of non-secure SID's in attach secure
		 */
		if (cam_smmu_send_syscall_pix_intf(VMID_CP_CAMERA, idx)) {
			pr_err("error: syscall failed\n");
			return -EINVAL;
		}
	}
	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		if (cam_smmu_attach(idx)) {
			pr_err("error: failed to attach\n");
			return -EINVAL;
		}
	}
	if (!iommu_cb_set.camera_secure_sid) {
		rc = msm_camera_tz_set_mode(MSM_CAMERA_TZ_MODE_SECURE,
			MSM_CAMERA_TZ_HW_BLOCK_ISP);
		if (rc != 0) {
			pr_err("secure mode TA notify vfe unsuccessful rc %d\n",
				rc);
			/*
			 * Although the TA notification failed, the flow should
			 * proceed without returning an error as at this point
			 * vfe had already entered the secure mode
			 */
		}
	}
	return 0;
}

static int cam_smmu_detach_sec_vfe_ns_stats(int idx)
{
	int32_t rc = 0;

	if (!iommu_cb_set.camera_secure_sid) {
		rc = msm_camera_tz_set_mode(MSM_CAMERA_TZ_MODE_NON_SECURE,
			MSM_CAMERA_TZ_HW_BLOCK_ISP);
		if (rc != 0) {
			pr_err("secure mode TA notify vfe unsuccessful rc %d\n",
				rc);
			/*
			 * Although the TA notification failed, the flow should
			 * proceed without returning an error, as at this point
			 * vfe is in secure mode and should be switched to
			 * non-secure regardless
			 */
		}
	}
	/*
	 *While exiting from secure mode for secure pix and non-secure stats,
	 *localizing detach/scm of non-secure SID's to detach secure
	 */
	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_DETACH) {
		if (cam_smmu_detach_device(idx) < 0) {
			pr_err("Error: ARM IOMMU detach failed\n");
			return -ENODEV;
		}
	}
	if (!iommu_cb_set.camera_secure_sid) {
		if (cam_smmu_send_syscall_pix_intf(VMID_HLOS, idx)) {
			pr_err("error: syscall failed\n");
			return -EINVAL;
		}
	}
	return 0;
}

static int cam_smmu_map_buffer_and_add_to_list(int idx, int ion_fd,
		 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
		 size_t *len_ptr)
{
	int rc = -1;
	struct cam_dma_buff_info *mapping_info;
	struct dma_buf *buf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;

	if (!paddr_ptr) {
		rc = -EINVAL;
		goto err_out;
	}

	/* allocate memory for each buffer information */
	buf = dma_buf_get(ion_fd);
	if (IS_ERR_OR_NULL(buf)) {
		rc = PTR_ERR(buf);
		pr_err("Error: dma get buf failed. fd = %d rc = %d\n",
		      ion_fd, rc);
		goto err_out;
	}

	attach = dma_buf_attach(buf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		rc = PTR_ERR(attach);
		pr_err("Error: dma buf attach failed\n");
		goto err_put;
	}

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		rc = PTR_ERR(table);
		pr_err("Error: dma buf map attachment failed\n");
		goto err_detach;
	}

	if (table->sgl) {
		CDBG("DMA buf: %pK, device: %pK, attach: %pK, table: %pK\n",
				(void *)buf,
				(void *)iommu_cb_set.cb_info[idx].dev,
				(void *)attach, (void *)table);
		CDBG("table sgl: %pK, rc: %d, dma_address: 0x%x\n",
				(void *)table->sgl, rc,
				(unsigned int)table->sgl->dma_address);
	} else {
		rc = -EINVAL;
		pr_err("Error: table sgl is null\n");
		goto err_unmap_sg;
	}

	/* fill up mapping_info */
	mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (!mapping_info) {
		rc = -ENOSPC;
		goto err_unmap_sg;
	}
	mapping_info->ion_fd = ion_fd;
	mapping_info->buf = buf;
	mapping_info->attach = attach;
	mapping_info->table = table;
	mapping_info->paddr = sg_dma_address(table->sgl);
	mapping_info->len = (size_t)buf->size;
	mapping_info->dir = dma_dir;
	mapping_info->ref_count = 1;

	/* return paddr and len to client */
	*paddr_ptr = sg_dma_address(table->sgl);
	*len_ptr = (size_t)buf->size;

	if (!*paddr_ptr || !*len_ptr) {
		pr_err("Error: Space Allocation failed!\n");
		rc = -ENOSPC;
		goto err_mapping_info;
	}
	CDBG("name %s ion_fd = %d, dev = %pK, paddr= %pK, len = %u\n",
			iommu_cb_set.cb_info[idx].name,
			ion_fd,
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)*paddr_ptr, (unsigned int)*len_ptr);

	/* add to the list */
	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].smmu_buf_list);
	return 0;

err_mapping_info:
	kfree(mapping_info);
err_unmap_sg:
	dma_buf_unmap_attachment(attach, table, dma_dir);
err_detach:
	dma_buf_detach(buf, attach);
err_put:
	dma_buf_put(buf);
err_out:
	return rc;
}

static int cam_smmu_unmap_buf_and_remove_from_list(
		struct cam_dma_buff_info *mapping_info,
		int idx)
{
	if ((!mapping_info->buf) || (!mapping_info->table) ||
		(!mapping_info->attach)) {
		pr_err("Error: Invalid params dev = %pK, table = %pK\n",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		pr_err("Error:dma_buf = %pK, attach = %pK\n",
			(void *)mapping_info->buf,
			(void *)mapping_info->attach);
		return -EINVAL;
	}

	/* iommu buffer clean up */
	dma_buf_unmap_attachment(mapping_info->attach,
		mapping_info->table, mapping_info->dir);
	dma_buf_detach(mapping_info->buf, mapping_info->attach);
	dma_buf_put(mapping_info->buf);
	mapping_info->buf = NULL;

	list_del_init(&mapping_info->list);

	/* free one buffer */
	kfree(mapping_info);
	return 0;
}

static enum cam_smmu_buf_state cam_smmu_check_fd_in_list(int idx,
					int ion_fd, dma_addr_t *paddr_ptr,
					size_t *len_ptr)
{
	struct cam_dma_buff_info *mapping;

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
			mapping->ref_count++;
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}
	return CAM_SMMU_BUFF_NOT_EXIST;
}

static enum cam_smmu_buf_state cam_smmu_check_secure_fd_in_list(int idx,
					int ion_fd, dma_addr_t *paddr_ptr,
					size_t *len_ptr)
{
	struct cam_sec_buff_info *mapping;

	list_for_each_entry(mapping,
			&iommu_cb_set.cb_info[idx].smmu_buf_list,
			list) {
		if (mapping->ion_fd == ion_fd) {
			mapping->ref_count++;
			*paddr_ptr = mapping->paddr;
			*len_ptr = mapping->len;
			return CAM_SMMU_BUFF_EXIST;
		}
	}
	return CAM_SMMU_BUFF_NOT_EXIST;
}

int cam_smmu_get_handle(char *identifier, int *handle_ptr)
{
	int ret = 0;

	if (!identifier) {
		pr_err("Error: iommu hardware name is NULL\n");
		return -EFAULT;
	}

	if (!handle_ptr) {
		pr_err("Error: handle pointer is NULL\n");
		return -EFAULT;
	}

	/* create and put handle in the table */
	ret = cam_smmu_create_add_handle_in_table(identifier, handle_ptr);
	if (ret < 0) {
		pr_err("Error: %s get handle fail\n", identifier);
		return ret;
	}
	return ret;
}
EXPORT_SYMBOL(cam_smmu_get_handle);


int cam_smmu_set_attr(int handle, uint32_t flags, int32_t *data)
{
	int ret = 0, idx;
	struct cam_context_bank_info *cb = NULL;
	struct iommu_domain *domain = NULL;

	CDBG("E: set_attr\n");
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}
	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].state == CAM_SMMU_DETACH) {
		domain = iommu_cb_set.cb_info[idx].domain;
		cb = &iommu_cb_set.cb_info[idx];
		cb->attr |= flags;
		/* set attributes */
		ret = iommu_domain_set_attr(domain, cb->attr, (void *)data);
		if (ret < 0) {
			mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
			pr_err("Error: set attr\n");
			return -ENODEV;
		}
	} else {
		ret = -EINVAL;
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return ret;
}
EXPORT_SYMBOL(cam_smmu_set_attr);


int cam_smmu_ops(int handle, enum cam_smmu_ops_param ops)
{
	int ret = 0, idx;

	CDBG("E: ops = %d\n", ops);
	if (iommu_cb_set.camera_secure_sid)
		return ret;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	switch (ops) {
	case CAM_SMMU_ATTACH: {
		ret = cam_smmu_attach(idx);
		break;
	}
	case CAM_SMMU_DETACH: {
		ret = cam_smmu_detach_device(idx);
		break;
	}
	case CAM_SMMU_ATTACH_SEC_VFE_NS_STATS: {
		ret = cam_smmu_attach_sec_vfe_ns_stats(idx);
		break;
	}
	case CAM_SMMU_DETACH_SEC_VFE_NS_STATS: {
		ret = cam_smmu_detach_sec_vfe_ns_stats(idx);
		break;
	}
	case CAM_SMMU_ATTACH_SEC_CPP: {
		ret = cam_smmu_attach_sec_cpp(idx);
		break;
	}
	case CAM_SMMU_DETACH_SEC_CPP: {
		ret = cam_smmu_detach_sec_cpp(idx);
		break;
	}
	case CAM_SMMU_VOTE:
	case CAM_SMMU_DEVOTE:
	default:
		pr_err("Error: idx = %d, ops = %d\n", idx, ops);
		ret = -EINVAL;
	}
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return ret;
}
EXPORT_SYMBOL(cam_smmu_ops);

static int cam_smmu_alloc_scratch_buffer_add_to_list(int idx,
					      size_t virt_len,
					      size_t phys_len,
					      unsigned int iommu_dir,
					      dma_addr_t *virt_addr)
{
	unsigned long nents = virt_len / phys_len;
	struct cam_dma_buff_info *mapping_info = NULL;
	size_t unmapped;
	dma_addr_t iova = 0;
	struct scatterlist *sg;
	int i = 0;
	int rc;
	struct iommu_domain *domain = NULL;
	struct page *page;
	struct sg_table *table = NULL;

	CDBG("%s: nents = %lu, idx = %d, virt_len  = %zx\n",
		__func__, nents, idx, virt_len);
	CDBG("%s: phys_len = %zx, iommu_dir = %d, virt_addr = %pK\n",
		__func__, phys_len, iommu_dir, virt_addr);

	/* This table will go inside the 'mapping' structure
	 * where it will be held until put_scratch_buffer is called
	 */
	table = kzalloc(sizeof(struct sg_table), GFP_KERNEL);
	if (!table) {
		rc = -ENOMEM;
		goto err_table_alloc;
	}

	rc = sg_alloc_table(table, nents, GFP_KERNEL);
	if (rc < 0) {
		rc = -EINVAL;
		goto err_sg_alloc;
	}

	page = alloc_pages(GFP_KERNEL, get_order(phys_len));
	if (!page) {
		rc = -ENOMEM;
		goto err_page_alloc;
	}

	/* Now we create the sg list */
	for_each_sg(table->sgl, sg, table->nents, i)
		sg_set_page(sg, page, phys_len, 0);


	/* Get the domain from within our cb_set struct and map it*/
	domain = iommu_cb_set.cb_info[idx].domain;

	rc = cam_smmu_alloc_scratch_va(&iommu_cb_set.cb_info[idx].scratch_map,
					virt_len, &iova);

	if (rc < 0) {
		pr_err("Could not find valid iova for scratch buffer\n");
		goto err_iommu_map;
	}

	if (iommu_map_sg(domain,
			  iova,
			  table->sgl,
			  table->nents,
			  iommu_dir) != virt_len) {
		pr_err("iommu_map_sg() failed\n");
		goto err_iommu_map;
	}

	/* Now update our mapping information within the cb_set struct */
	mapping_info = kzalloc(sizeof(struct cam_dma_buff_info), GFP_KERNEL);
	if (!mapping_info) {
		rc = -ENOMEM;
		goto err_mapping_info;
	}

	mapping_info->ion_fd = 0xDEADBEEF;
	mapping_info->buf = NULL;
	mapping_info->attach = NULL;
	mapping_info->table = table;
	mapping_info->paddr = iova;
	mapping_info->len = virt_len;
	mapping_info->iommu_dir = iommu_dir;
	mapping_info->ref_count = 1;
	mapping_info->phys_len = phys_len;

	CDBG("%s: paddr = %pK, len = %zx, phys_len = %zx",
		__func__, (void *)mapping_info->paddr,
		mapping_info->len, mapping_info->phys_len);

	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].smmu_buf_list);

	*virt_addr = (dma_addr_t)iova;

	CDBG("%s: mapped virtual address = %lx\n", __func__,
		(unsigned long)*virt_addr);
	return 0;

err_mapping_info:
	unmapped = iommu_unmap(domain, iova,  virt_len);
	if (unmapped != virt_len)
		pr_err("Unmapped only %zx instead of %zx\n",
			unmapped, virt_len);
err_iommu_map:
	__free_pages(sg_page(table->sgl), get_order(phys_len));
err_page_alloc:
	sg_free_table(table);
err_sg_alloc:
	kfree(table);
err_table_alloc:
	return rc;
}

static int cam_smmu_free_scratch_buffer_remove_from_list(
					struct cam_dma_buff_info *mapping_info,
					int idx)
{
	int rc = 0;
	size_t unmapped;
	struct iommu_domain *domain =
		iommu_cb_set.cb_info[idx].domain;
	struct scratch_mapping *scratch_map =
		&iommu_cb_set.cb_info[idx].scratch_map;

	if (!mapping_info->table) {
		pr_err("Error: Invalid params: dev = %pK, table = %pK,\n",
				(void *)iommu_cb_set.cb_info[idx].dev,
				(void *)mapping_info->table);
		return -EINVAL;
	}

	/* Clean up the mapping_info struct from the list */
	unmapped = iommu_unmap(domain, mapping_info->paddr, mapping_info->len);
	if (unmapped != mapping_info->len)
		pr_err("Unmapped only %zx instead of %zx\n",
				unmapped, mapping_info->len);

	rc = cam_smmu_free_scratch_va(scratch_map,
				mapping_info->paddr,
				mapping_info->len);
	if (rc < 0) {
		pr_err("Error: Invalid iova while freeing scratch buffer\n");
		rc = -EINVAL;
	}

	__free_pages(sg_page(mapping_info->table->sgl),
			get_order(mapping_info->phys_len));
	sg_free_table(mapping_info->table);
	kfree(mapping_info->table);
	list_del_init(&mapping_info->list);

	kfree(mapping_info);
	mapping_info = NULL;

	return rc;
}

int cam_smmu_get_phy_addr_scratch(int handle,
				  enum cam_smmu_map_dir dir,
				  dma_addr_t *paddr_ptr,
				  size_t virt_len,
				  size_t phys_len)
{
	int idx, rc = 0;
	unsigned int iommu_dir;

	if (!paddr_ptr || !virt_len || !phys_len) {
		pr_err("Error: Input pointer or lengths invalid\n");
		return -EINVAL;
	}

	if (virt_len < phys_len) {
		pr_err("Error: virt_len > phys_len\n");
		return -EINVAL;
	}

	iommu_dir = cam_smmu_translate_dir_to_iommu_dir(dir);
	if (iommu_dir == IOMMU_INVALID_DIR) {
		pr_err("Error: translate direction failed. dir = %d\n", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto error;
	}

	if (!iommu_cb_set.cb_info[idx].scratch_buf_support) {
		pr_err("Error: Context bank does not support scratch bufs\n");
		rc = -EINVAL;
		goto error;
	}

	CDBG("%s: smmu handle = %x, idx = %d, dir = %d\n",
		__func__, handle, idx, dir);
	CDBG("%s: virt_len = %zx, phys_len  = %zx\n",
		__func__, phys_len, virt_len);

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto error;
	}

	if (!IS_ALIGNED(virt_len, PAGE_SIZE)) {
		pr_err("Requested scratch buffer length not page aligned\n");
		rc = -EINVAL;
		goto error;
	}

	if (!IS_ALIGNED(virt_len, phys_len)) {
		pr_err("Requested virtual length not aligned with physical length\n");
		rc = -EINVAL;
		goto error;
	}

	rc = cam_smmu_alloc_scratch_buffer_add_to_list(idx,
							virt_len,
							phys_len,
							iommu_dir,
							paddr_ptr);
	if (rc < 0) {
		pr_err("Error: mapping or add list fail\n");
		goto error;
	}

error:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}

int cam_smmu_put_phy_addr_scratch(int handle,
				  dma_addr_t paddr)
{
	int idx;
	int rc = -1;
	struct cam_dma_buff_info *mapping_info;

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto handle_err;
	}

	if (!iommu_cb_set.cb_info[idx].scratch_buf_support) {
		pr_err("Error: Context bank does not support scratch buffers\n");
		rc = -EINVAL;
		goto handle_err;
	}

	/* Based on virtual address and index, we can find mapping info
	 * of the scratch buffer
	 */
	mapping_info = cam_smmu_find_mapping_by_virt_address(idx, paddr);
	if (!mapping_info) {
		pr_err("Error: Invalid params\n");
		rc = -EINVAL;
		goto handle_err;
	}

	/* unmapping one buffer from device */
	rc = cam_smmu_free_scratch_buffer_remove_from_list(mapping_info, idx);
	if (rc < 0) {
		pr_err("Error: unmap or remove list fail\n");
		goto handle_err;
	}
handle_err:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}

int cam_smmu_alloc_get_stage2_scratch_mem(int handle,
		enum cam_smmu_map_dir dir, struct dma_buf **dmabuf,
		dma_addr_t *addr, size_t *len_ptr)
{
	int idx, rc = 0;
	enum dma_data_direction dma_dir;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		pr_err("Error: translate direction failed. dir = %d\n", dir);
		return -EINVAL;
	}
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		return -EINVAL;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		return -EINVAL;
	}
	*dmabuf = ion_alloc(SZ_2M, ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID),
				ION_FLAG_SECURE | ION_FLAG_CP_CAMERA);
	if (IS_ERR_OR_NULL(*dmabuf))
		return -ENOMEM;

	attach = dma_buf_attach(*dmabuf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_err("Error: dma buf attach failed\n");
		rc = PTR_ERR(attach);
		goto err_put;
	}

	attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("Error: dma buf map attachment failed\n");
		rc = PTR_ERR(table);
		goto err_detach;
	}

	/* return addr and len to client */
	*addr = sg_phys(table->sgl);
	*len_ptr = (size_t)sg_dma_len(table->sgl);

	CDBG("dev = %pK, paddr= %pK, len = %u\n",
		(void *)iommu_cb_set.cb_info[idx].dev,
		(void *)*addr, (unsigned int)*len_ptr);
	return rc;

err_detach:
	dma_buf_detach(*dmabuf, attach);
err_put:
	dma_buf_put(*dmabuf);

	return rc;
}

int cam_smmu_free_stage2_scratch_mem(int handle, struct dma_buf *dmabuf)
{
	int idx = 0;
	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}
	dma_buf_put(dmabuf);
	return 0;
}

static int cam_smmu_secure_unmap_buf_and_remove_from_list(
		struct cam_sec_buff_info *mapping_info,
		int idx)
{
	if ((!mapping_info->dmabuf) || (!mapping_info->table) ||
		(!mapping_info->attach)) {
		pr_err("Error: Invalid params dev = %pK, table = %pK\n",
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)mapping_info->table);
		pr_err("Error:dma_buf = %pK, attach = %pK\n",
			(void *)mapping_info->dmabuf,
			(void *)mapping_info->attach);
		return -EINVAL;
	}

	/* skip cache operations */
	mapping_info->attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	/* iommu buffer clean up */
	dma_buf_unmap_attachment(mapping_info->attach,
	mapping_info->table, mapping_info->dir);
	dma_buf_detach(mapping_info->dmabuf, mapping_info->attach);
	dma_buf_put(mapping_info->dmabuf);
	mapping_info->dmabuf = NULL;

	list_del_init(&mapping_info->list);

	/* free one buffer */
	kfree(mapping_info);
	return 0;
}

int cam_smmu_put_stage2_phy_addr(int handle, int ion_fd)
{
	int idx, rc;
	struct cam_sec_buff_info *mapping_info;

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto put_addr_end;
	}

	/* based on ion fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_sec_buf_idx(idx, ion_fd);
	if (!mapping_info) {
		pr_err("Error: Invalid params! idx = %d, fd = %d\n",
			idx, ion_fd);
		rc = -EINVAL;
		goto put_addr_end;
	}

	mapping_info->ref_count--;
	if (mapping_info->ref_count > 0) {
		CDBG("There are still %u buffer(s) with same fd %d",
			mapping_info->ref_count, mapping_info->ion_fd);
		rc = 0;
		goto put_addr_end;
	}

	/* unmapping one buffer from device */
	rc = cam_smmu_secure_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0) {
		pr_err("Error: unmap or remove list fail\n");
		goto put_addr_end;
	}

put_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_put_stage2_phy_addr);

static int cam_smmu_map_stage2_buffer_and_add_to_list(int idx, int ion_fd,
		 enum dma_data_direction dma_dir, dma_addr_t *paddr_ptr,
		 size_t *len_ptr)
{
	int rc = 0;
	struct dma_buf *dmabuf = NULL;
	struct dma_buf_attachment *attach = NULL;
	struct sg_table *table = NULL;
	struct cam_sec_buff_info *mapping_info;


	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		return -EINVAL;
	}

	dmabuf = dma_buf_get(ion_fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		pr_err("Error: dma buf get failed\n");
		return -EINVAL;
	}

	/*
	 * ion_phys() is deprecated. call dma_buf_attach() and
	 * dma_buf_map_attachment() to get the buffer's physical
	 * address.
	 */
	attach = dma_buf_attach(dmabuf, iommu_cb_set.cb_info[idx].dev);
	if (IS_ERR_OR_NULL(attach)) {
		pr_err("Error: dma buf attach failed\n");
		return -EINVAL;
	}

	attach->dma_map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	table = dma_buf_map_attachment(attach, dma_dir);
	if (IS_ERR_OR_NULL(table)) {
		pr_err("Error: dma buf map attachment failed\n");
		return -EINVAL;
	}

	/* return addr and len to client */
	*paddr_ptr = sg_phys(table->sgl);
	*len_ptr = (size_t)sg_dma_len(table->sgl);

	/* fill up mapping_info */
	mapping_info = kzalloc(sizeof(struct cam_sec_buff_info), GFP_KERNEL);
	if (!mapping_info)
		return -ENOSPC;

	mapping_info->ion_fd = ion_fd;
	mapping_info->paddr = *paddr_ptr;
	mapping_info->len = *len_ptr;
	mapping_info->dir = dma_dir;
	mapping_info->ref_count = 1;
	mapping_info->dmabuf = dmabuf;
	mapping_info->attach = attach;
	mapping_info->table = table;

	CDBG("ion_fd = %d, dev = %pK, paddr= %pK, len = %u\n", ion_fd,
			(void *)iommu_cb_set.cb_info[idx].dev,
			(void *)*paddr_ptr, (unsigned int)*len_ptr);

	/* add to the list */
	list_add(&mapping_info->list, &iommu_cb_set.cb_info[idx].smmu_buf_list);

	return rc;
}

int cam_smmu_get_stage2_phy_addr(int handle,
		int ion_fd, enum cam_smmu_map_dir dir,
		dma_addr_t *paddr_ptr,
		size_t *len_ptr)
{
	int idx, rc;
	enum dma_data_direction dma_dir;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		pr_err("Error: Input pointers are invalid\n");
		return -EINVAL;
	}
	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		pr_err("Error: translate direction failed. dir = %d\n", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_secure_fd_in_list(idx, ion_fd, paddr_ptr,
			len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CDBG("ion_fd:%d already in the list, give same addr back",
				 ion_fd);
		rc = 0;
		goto get_addr_end;
	}
	rc = cam_smmu_map_stage2_buffer_and_add_to_list(idx, ion_fd, dma_dir,
			paddr_ptr, len_ptr);
	if (rc < 0) {
		pr_err("Error: mapping or add list fail\n");
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}


int cam_smmu_get_phy_addr(int handle, int ion_fd,
		enum cam_smmu_map_dir dir, dma_addr_t *paddr_ptr,
		size_t *len_ptr)
{
	int idx, rc;
	enum dma_data_direction dma_dir;
	enum cam_smmu_buf_state buf_state;

	if (!paddr_ptr || !len_ptr) {
		pr_err("Error: Input pointers are invalid\n");
		return -EINVAL;
	}
	/* clean the content from clients */
	*paddr_ptr = (dma_addr_t)NULL;
	*len_ptr = (size_t)0;

	dma_dir = cam_smmu_translate_dir(dir);
	if (dma_dir == DMA_NONE) {
		pr_err("Error: translate direction failed. dir = %d\n", dir);
		return -EINVAL;
	}

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto get_addr_end;
	}

	if (iommu_cb_set.cb_info[idx].state != CAM_SMMU_ATTACH) {
		pr_err("Error: Device %s should call SMMU attach before map buffer\n",
				iommu_cb_set.cb_info[idx].name);
		rc = -EINVAL;
		goto get_addr_end;
	}

	buf_state = cam_smmu_check_fd_in_list(idx, ion_fd, paddr_ptr, len_ptr);
	if (buf_state == CAM_SMMU_BUFF_EXIST) {
		CDBG("ion_fd:%d already in the list, give same addr back",
				 ion_fd);
		rc = 0;
		goto get_addr_end;
	}
	rc = cam_smmu_map_buffer_and_add_to_list(idx, ion_fd, dma_dir,
			paddr_ptr, len_ptr);
	if (rc < 0) {
		pr_err("Error: mapping or add list fail\n");
		goto get_addr_end;
	}

get_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_get_phy_addr);

int cam_smmu_put_phy_addr(int handle, int ion_fd)
{
	int idx, rc;
	struct cam_dma_buff_info *mapping_info;

	/* find index in the iommu_cb_set.cb_info */
	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		rc = -EINVAL;
		goto put_addr_end;
	}

	/* based on ion fd and index, we can find mapping info of buffer */
	mapping_info = cam_smmu_find_mapping_by_ion_index(idx, ion_fd);
	if (!mapping_info) {
		pr_err("Error: Invalid params! idx = %d, fd = %d\n",
			idx, ion_fd);
		rc = -EINVAL;
		goto put_addr_end;
	}

	mapping_info->ref_count--;
	if (mapping_info->ref_count > 0) {
		CDBG("There are still %u buffer(s) with same fd %d",
			mapping_info->ref_count, mapping_info->ion_fd);
		rc = 0;
		goto put_addr_end;
	}

	/* unmapping one buffer from device */
	rc = cam_smmu_unmap_buf_and_remove_from_list(mapping_info, idx);
	if (rc < 0) {
		pr_err("Error: unmap or remove list fail\n");
		goto put_addr_end;
	}

put_addr_end:
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return rc;
}
EXPORT_SYMBOL(cam_smmu_put_phy_addr);

int cam_smmu_destroy_handle(int handle)
{
	int idx;

	idx = GET_SMMU_TABLE_IDX(handle);
	if (handle == HANDLE_INIT || idx < 0 || idx >= iommu_cb_set.cb_num) {
		pr_err("Error: handle or index invalid. idx = %d hdl = %x\n",
			idx, handle);
		return -EINVAL;
	}

	mutex_lock(&iommu_cb_set.cb_info[idx].lock);
	if (--iommu_cb_set.cb_info[idx].ref_cnt != 0) {
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return 0;
	}

	if (iommu_cb_set.cb_info[idx].handle != handle) {
		pr_err("Error: hdl is not valid, table_hdl = %x, hdl = %x\n",
			iommu_cb_set.cb_info[idx].handle, handle);
		mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
		return -EINVAL;
	}

	if (!list_empty_careful(&iommu_cb_set.cb_info[idx].smmu_buf_list)) {
		pr_err("Client %s buffer list is not clean!\n",
			iommu_cb_set.cb_info[idx].name);
		cam_smmu_print_list(idx);
		cam_smmu_clean_buffer_list(idx);
	}

	iommu_cb_set.cb_info[idx].cb_count = 0;
	iommu_cb_set.cb_info[idx].handle = HANDLE_INIT;
	mutex_unlock(&iommu_cb_set.cb_info[idx].lock);
	return 0;
}
EXPORT_SYMBOL(cam_smmu_destroy_handle);

/*This function can only be called after smmu driver probe*/
int cam_smmu_get_num_of_clients(void)
{
	return iommu_cb_set.cb_num;
}

static void cam_smmu_release_cb(struct platform_device *pdev)
{
	int i = 0;

	for (i = 0; i < iommu_cb_set.cb_num; i++) {
		iommu_detach_device(iommu_cb_set.cb_info[i].domain,
				iommu_cb_set.cb_info[i].dev);
	}

	iommu_cb_set.cb_num = 0;
}

static int cam_smmu_setup_cb(struct cam_context_bank_info *cb,
	struct device *dev)
{
	int rc = 0;

	if (!cb || !dev) {
		pr_err("Error: invalid input params\n");
		return -EINVAL;
	}

	cb->dev = dev;
	/* Reserve 256M if scratch buffer support is desired
	 * and initialize the scratch mapping structure
	 */
	if (cb->scratch_buf_support) {
		cb->va_start = SCRATCH_ALLOC_END;
		cb->va_len = VA_SPACE_END - SCRATCH_ALLOC_END;

		rc = cam_smmu_init_scratch_map(&cb->scratch_map,
				SCRATCH_ALLOC_START,
				SCRATCH_ALLOC_END - SCRATCH_ALLOC_START,
				0);
		if (rc < 0) {
			pr_err("Error: failed to create scratch map\n");
			rc = -ENODEV;
			goto end;
		}
	} else {
		cb->va_start = SZ_128K;
		cb->va_len = VA_SPACE_END - SZ_128M;
	}
	/* create a virtual mapping */
	cb->domain = iommu_get_domain_for_dev(cb->dev);
	if (IS_ERR(cb->domain)) {
		pr_err("iommu get domain for dev: %s failed\n",
			dev_name(cb->dev));
		rc = -ENODEV;
		goto end;
	}

	return 0;
end:
	return rc;
}

static int cam_smmu_populate_sids(struct device *dev,
	struct cam_context_bank_info *cb)
{
	int i, j, rc = 0;
	unsigned int cnt = 0, iommu_cells_cnt = 0;
	const void *property;

	/* set the name of the context bank */
	property = of_get_property(dev->of_node, "iommus", &cnt);
	cnt /= 4;
	rc = of_property_read_u32_index(dev->of_node, "iommu-cells", 0,
		&iommu_cells_cnt);
	if (rc < 0)
		pr_err("invalid iommu-cells count : %d\n", rc);
	iommu_cells_cnt++;
	for (i = 0, j = 0; i < cnt; i = i + iommu_cells_cnt, j++) {
		rc = of_property_read_u32_index(dev->of_node,
			"iommus", i + 1, &cb->sids[j]);
		if (rc < 0)
			pr_err("misconfiguration, can't fetch SID\n");

		pr_err("__debug cnt = %d, cb->name: :%s sid [%d] = %d\n",
			cnt, cb->name, j, cb->sids[j]);
	}
	return rc;
}

static int cam_alloc_smmu_context_banks(struct device *dev)
{
	struct device_node *domains_child_node = NULL;

	if (!dev) {
		pr_err("Error: Invalid device\n");
		return -ENODEV;
	}

	iommu_cb_set.cb_num = 0;

	/* traverse thru all the child nodes and increment the cb count */
	for_each_child_of_node(dev->of_node, domains_child_node) {
		if (of_device_is_compatible(domains_child_node,
			"qcom,msm-cam-smmu-cb"))
			iommu_cb_set.cb_num++;

		if (of_device_is_compatible(domains_child_node,
			"qcom,qsmmu-cam-cb"))
			iommu_cb_set.cb_num++;
	}

	if (iommu_cb_set.cb_num == 0) {
		pr_err("Error: no context banks present\n");
		return -ENOENT;
	}

	/* allocate memory for the context banks */
	iommu_cb_set.cb_info = devm_kzalloc(dev,
		iommu_cb_set.cb_num * sizeof(struct cam_context_bank_info),
		GFP_KERNEL);

	if (!iommu_cb_set.cb_info) {
		pr_err("Error: cannot allocate context banks\n");
		return -ENOMEM;
	}

	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_INIT);
	iommu_cb_set.cb_init_count = 0;

	CDBG("no of context banks :%d\n", iommu_cb_set.cb_num);
	return 0;
}

static int cam_populate_smmu_context_banks(struct device *dev,
	enum cam_iommu_type type)
{
	int rc = 0;
	struct cam_context_bank_info *cb;
	struct device *ctx;

	if (!dev) {
		pr_err("Error: Invalid device\n");
		return -ENODEV;
	}

	/* check the bounds */
	if (iommu_cb_set.cb_init_count >= iommu_cb_set.cb_num) {
		pr_err("Error: populate more than allocated cb\n");
		rc = -EBADHANDLE;
		goto cb_init_fail;
	}

	/* read the context bank from cb set */
	cb = &iommu_cb_set.cb_info[iommu_cb_set.cb_init_count];

	/* set the name of the context bank */
	rc = of_property_read_string(dev->of_node, "label", &cb->name);
	if (rc) {
		pr_err("Error: failed to read label from sub device\n");
		goto cb_init_fail;
	}

	/* populate SID's for each cb */
	rc = cam_smmu_populate_sids(dev, cb);
	if (rc < 0)
		pr_err("Error: failed to populate sids : %s\n", cb->name);

	/* Check if context bank supports scratch buffers */
	if (of_property_read_bool(dev->of_node, "qcom,scratch-buf-support"))
		cb->scratch_buf_support = 1;
	else
		cb->scratch_buf_support = 0;

	/* set the secure/non secure domain type */
	if (of_property_read_bool(dev->of_node, "qcom,secure-context"))
		cb->is_secure = true;
	else
		cb->is_secure = false;

	CDBG("cb->name :%s, cb->is_secure :%d, cb->scratch_support :%d\n",
			cb->name, cb->is_secure, cb->scratch_buf_support);

	/* set up the iommu mapping for the  context bank */
	if (type == CAM_QSMMU) {
		pr_err("Error: QSMMU ctx not supported for: %s\n", cb->name);
		return -EINVAL;
	}
		ctx = dev;
		CDBG("getting Arm SMMU ctx : %s\n", cb->name);

	rc = cam_smmu_setup_cb(cb, ctx);
	if (rc < 0)
		pr_err("Error: failed to setup cb : %s\n", cb->name);

	iommu_set_fault_handler(cb->domain,
			cam_smmu_iommu_fault_handler,
			(void *)cb->name);

	if (iommu_cb_set.camera_secure_sid) {
		rc = cam_smmu_attach(iommu_cb_set.cb_init_count);
		if (rc)
			pr_err("Error: SMMU attach failed for %s\n", cb->name);
	}

	/* increment count to next bank */
	iommu_cb_set.cb_init_count++;

	CDBG("X: cb init count :%d\n", iommu_cb_set.cb_init_count);
	return rc;

cb_init_fail:
	iommu_cb_set.cb_info = NULL;
	return rc;
}

static int cam_smmu_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct device *dev = &pdev->dev;

	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu")) {
		rc = cam_alloc_smmu_context_banks(dev);
		if (rc < 0)	{
			pr_err("Error: allocating context banks\n");
			return -ENOMEM;
		}
	}
	if (of_device_is_compatible(dev->of_node, "qcom,msm-cam-smmu-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_ARM_SMMU);
		if (rc < 0) {
			pr_err("Error: populating context banks\n");
			return -ENOMEM;
		}
		return rc;
	}
	if (of_device_is_compatible(dev->of_node, "qcom,qsmmu-cam-cb")) {
		rc = cam_populate_smmu_context_banks(dev, CAM_QSMMU);
		if (rc < 0) {
			pr_err("Error: populating context banks\n");
			return -ENOMEM;
		}
		return rc;
	}

	if (of_property_read_bool(dev->of_node, "qcom,camera-secure-sid"))
		iommu_cb_set.camera_secure_sid = true;
	else
		iommu_cb_set.camera_secure_sid = false;

	/* probe thru all the subdevices */
	rc = of_platform_populate(pdev->dev.of_node, msm_cam_smmu_dt_match,
				NULL, &pdev->dev);
	if (rc < 0)
		pr_err("Error: populating devices\n");

	INIT_WORK(&iommu_cb_set.smmu_work, cam_smmu_page_fault_work);
	mutex_init(&iommu_cb_set.payload_list_lock);
	INIT_LIST_HEAD(&iommu_cb_set.payload_list);

	return rc;
}

static int cam_smmu_remove(struct platform_device *pdev)
{
	/* release all the context banks and memory allocated */
	cam_smmu_reset_iommu_table(CAM_SMMU_TABLE_DEINIT);
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,msm-cam-smmu"))
		cam_smmu_release_cb(pdev);
	return 0;
}

static struct platform_driver cam_smmu_driver = {
	.probe = cam_smmu_probe,
	.remove = cam_smmu_remove,
	.driver = {
		.name = "msm_cam_smmu",
		.of_match_table = msm_cam_smmu_dt_match,
	},
};

static int __init cam_smmu_init_module(void)
{
	return platform_driver_register(&cam_smmu_driver);
}

static void __exit cam_smmu_exit_module(void)
{
	platform_driver_unregister(&cam_smmu_driver);
}

module_init(cam_smmu_init_module);
module_exit(cam_smmu_exit_module);
MODULE_DESCRIPTION("MSM Camera SMMU driver");
MODULE_LICENSE("GPL v2");
