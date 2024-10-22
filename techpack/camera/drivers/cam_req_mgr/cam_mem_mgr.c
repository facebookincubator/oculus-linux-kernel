// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <linux/debugfs.h>
#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
#if IS_REACHABLE(CONFIG_QCOM_MEM_BUF)
#include <linux/mem-buf.h>
#endif
#include <soc/qcom/secure_buffer.h>
#endif

#include "cam_compat.h"
#include "cam_req_mgr_util.h"
#include "cam_mem_mgr.h"
#include "cam_smmu_api.h"
#include "cam_debug_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_presil_hw_access.h"
#include "cam_compat.h"
#include "cam_fastrpc.h"

#define CAM_MEM_SHARED_BUFFER_PAD_4K (4 * 1024)

static struct cam_mem_table tbl;
static atomic_t cam_mem_mgr_state = ATOMIC_INIT(CAM_MEM_MGR_UNINITIALIZED);

#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
static void cam_mem_mgr_put_dma_heaps(void);
static int cam_mem_mgr_get_dma_heaps(void);
#endif
static void cam_mem_mgr_unmap_active_buf(int);

#ifdef CONFIG_CAM_PRESIL
static inline void cam_mem_mgr_reset_presil_params(int idx)
{
	tbl.bufq[idx].presil_params.fd_for_umd_daemon = -1;
	tbl.bufq[idx].presil_params.refcount = 0;
}
#else
static inline void cam_mem_mgr_reset_presil_params(int idx)
{
	return;
}
#endif

static unsigned long cam_mem_mgr_mini_dump_cb(void *dst, unsigned long len)
{
	struct cam_mem_table_mini_dump      *md;

	if (!dst) {
		CAM_ERR(CAM_MEM, "Invalid  params");
		return 0;
	}

	if (len < sizeof(*md)) {
		CAM_ERR(CAM_MEM, "Insufficient length %u", len);
		return 0;
	}

	md = (struct cam_mem_table_mini_dump *)dst;
	memcpy(md->bufq, tbl.bufq, CAM_MEM_BUFQ_MAX * sizeof(struct cam_mem_buf_queue));
	md->dbg_buf_idx = tbl.dbg_buf_idx;
	md->alloc_profile_enable = tbl.alloc_profile_enable;
	md->force_cache_allocs = tbl.force_cache_allocs;
	md->need_shared_buffer_padding = tbl.need_shared_buffer_padding;
	return sizeof(*md);
}

static void cam_mem_mgr_print_tbl(void)
{
	int i;
	uint64_t ms, hrs, min, sec;
	struct timespec64 current_ts;

	CAM_GET_TIMESTAMP(current_ts);
	CAM_CONVERT_TIMESTAMP_FORMAT(current_ts, hrs, min, sec, ms);

	CAM_INFO(CAM_MEM, "***%llu:%llu:%llu:%llu Mem mgr table dump***",
		hrs, min, sec, ms);
	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		if (tbl.bufq[i].active) {
			CAM_CONVERT_TIMESTAMP_FORMAT((tbl.bufq[i].timestamp), hrs, min, sec, ms);
			CAM_INFO(CAM_MEM,
				"%llu:%llu:%llu:%llu idx %d fd %d i_ino %lu size %llu krefCount %d urefCount %d",
				hrs, min, sec, ms, i, tbl.bufq[i].fd, tbl.bufq[i].i_ino,
				tbl.bufq[i].len, kref_read(&tbl.bufq[i].krefcount),
				kref_read(&tbl.bufq[i].urefcount));
		}
	}

}

static int cam_mem_util_get_dma_dir(uint32_t flags)
{
	int rc = -EINVAL;

	if (flags & CAM_MEM_FLAG_HW_READ_ONLY)
		rc = DMA_TO_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_WRITE_ONLY)
		rc = DMA_FROM_DEVICE;
	else if (flags & CAM_MEM_FLAG_HW_READ_WRITE)
		rc = DMA_BIDIRECTIONAL;
	else if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
		rc = DMA_BIDIRECTIONAL;

	return rc;
}

int cam_mem_mgr_release_nsp_buf(void)
{
	int i, rc = 0;
	struct cam_mem_mgr_release_cmd cmd;

	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		/*
		 * TODO: Check if we can avoid the use of new flag is_nsp_buf
		 * and use uapi flag itself to decide if the buffer is nsp buf
		 * or not.
		 */
		if ((tbl.bufq[i].active) && (tbl.bufq[i].is_nsp_buf)) {
			CAM_DBG(CAM_MEM,
				"idx %d fd %d i_ino %lu size %llu flag 0x%x",
				i, tbl.bufq[i].fd, tbl.bufq[i].i_ino,
				tbl.bufq[i].len, tbl.bufq[i].flags);
			cmd.buf_handle = tbl.bufq[i].buf_handle;
			rc = cam_mem_mgr_release(&cmd);
		}
	}

	return rc;
}

static int cam_mem_util_map_cpu_va(struct dma_buf *dmabuf, uintptr_t *vaddr, size_t *len)
{
	int rc = 0;

	/*
	 * dma_buf_begin_cpu_access() and dma_buf_end_cpu_access()
	 * need to be called in pair to avoid stability issue.
	 */
	rc = dma_buf_begin_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
		return rc;
	}

	rc = cam_compat_util_get_dmabuf_va(dmabuf, vaddr);
	if (rc) {
		CAM_ERR(CAM_MEM, "kernel vmap failed: rc = %d", rc);
		*len = 0;
		dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	}
	else {
		*len = dmabuf->size;
		CAM_DBG(CAM_MEM, "vaddr = %llu, len = %zu", *vaddr, *len);
	}

	return rc;
}

static int cam_mem_util_unmap_cpu_va(struct dma_buf *dmabuf,
	uint64_t vaddr)
{
	int rc = 0;

	if (!dmabuf || !vaddr) {
		CAM_ERR(CAM_MEM, "Invalid input args %pK %llX", dmabuf, vaddr);
		return -EINVAL;
	}

	cam_compat_util_put_dmabuf_va(dmabuf, (void *)vaddr);

	/*
	 * dma_buf_begin_cpu_access() and
	 * dma_buf_end_cpu_access() need to be called in pair
	 * to avoid stability issue.
	 */
	rc = dma_buf_end_cpu_access(dmabuf, DMA_BIDIRECTIONAL);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed in end cpu access, dmabuf=%pK",
			dmabuf);
		return rc;
	}

	return rc;
}

static int cam_mem_mgr_create_debug_fs(void)
{
	int rc = 0;
	struct dentry *dbgfileptr = NULL;

	dbgfileptr = debugfs_create_dir("camera_memmgr", NULL);
	if (!dbgfileptr) {
		CAM_ERR(CAM_MEM,"DebugFS could not create directory!");
		rc = -ENOENT;
		goto end;
	}
	/* Store parent inode for cleanup in caller */
	tbl.dentry = dbgfileptr;

	debugfs_create_bool("alloc_profile_enable", 0644,
		tbl.dentry, &tbl.alloc_profile_enable);
end:
	return rc;
}

int cam_mem_mgr_init(void)
{
	int i;
	int bitmap_size;
	int rc = 0;

	memset(tbl.bufq, 0, sizeof(tbl.bufq));

	if (cam_smmu_need_force_alloc_cached(&tbl.force_cache_allocs)) {
		CAM_ERR(CAM_MEM, "Error in getting force cache alloc flag");
		return -EINVAL;
	}

	tbl.need_shared_buffer_padding = cam_smmu_need_shared_buffer_padding();

#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
	rc = cam_mem_mgr_get_dma_heaps();
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed in getting dma heaps rc=%d", rc);
		return rc;
	}
#endif
	bitmap_size = BITS_TO_LONGS(CAM_MEM_BUFQ_MAX) * sizeof(long);
	tbl.bitmap = kzalloc(bitmap_size, GFP_KERNEL);
	if (!tbl.bitmap) {
		rc = -ENOMEM;
		goto put_heaps;
	}

	tbl.bits = bitmap_size * BITS_PER_BYTE;
	bitmap_zero(tbl.bitmap, tbl.bits);
	/* We need to reserve slot 0 because 0 is invalid */
	set_bit(0, tbl.bitmap);

	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		tbl.bufq[i].fd = -1;
		tbl.bufq[i].buf_handle = -1;
		cam_mem_mgr_reset_presil_params(i);
	}
	mutex_init(&tbl.m_lock);

	atomic_set(&cam_mem_mgr_state, CAM_MEM_MGR_INITIALIZED);

	mutex_init(&tbl.eb_tbl.eb_mutex);
	hash_init(tbl.eb_tbl.eb_map);

	cam_mem_mgr_create_debug_fs();
	cam_common_register_mini_dump_cb(cam_mem_mgr_mini_dump_cb,
		"cam_mem");

	return 0;
put_heaps:
#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
	cam_mem_mgr_put_dma_heaps();
#endif
	return rc;
}

static int32_t cam_mem_get_slot_for_dmabuf(struct dma_buf *dmabuf, bool *exist)
{
	int32_t idx;
	struct cam_mem_buf_queue *priv;

	if (!dmabuf)
		return -EINVAL;

	if (dmabuf && exist) {
		mutex_lock(&tbl.eb_tbl.eb_mutex);
		hash_for_each_possible(tbl.eb_tbl.eb_map, priv, hlist, PTR_TO_U64(dmabuf)) {
			if (dmabuf == priv->dma_buf) {
				idx = priv->bufq_idx;
				*exist = true;
				mutex_unlock(&tbl.eb_tbl.eb_mutex);
				CAM_DBG(CAM_MEM, "is_internal %d index %d dmabuf %pK",
					tbl.bufq[idx].is_internal, idx, dmabuf);
				return idx;
			}
		}
		mutex_unlock(&tbl.eb_tbl.eb_mutex);
	}

	mutex_lock(&tbl.m_lock);
	if (!tbl.bitmap) {
		CAM_ERR(CAM_MEM, "crm is closing and mem deinit");
		mutex_unlock(&tbl.m_lock);
		return -EINVAL;
	}
	idx = find_first_zero_bit(tbl.bitmap, tbl.bits);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		mutex_unlock(&tbl.m_lock);
		return -ENOMEM;
	}

	set_bit(idx, tbl.bitmap);
	tbl.bufq[idx].active = true;
	CAM_GET_TIMESTAMP((tbl.bufq[idx].timestamp));
	mutex_init(&tbl.bufq[idx].q_lock);
	mutex_init(&tbl.bufq[idx].ref_lock);
	mutex_unlock(&tbl.m_lock);

	return idx;
}

static void cam_mem_put_slot(int32_t idx)
{
	mutex_lock(&tbl.m_lock);
	if (!tbl.bitmap) {
		CAM_ERR(CAM_MEM, "crm is closing and mem deinit");
		mutex_unlock(&tbl.m_lock);
		return;
	}
	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].active = false;
	tbl.bufq[idx].is_internal = false;
	memset(&tbl.bufq[idx].timestamp, 0, sizeof(struct timespec64));
	mutex_unlock(&tbl.bufq[idx].q_lock);
	mutex_lock(&tbl.bufq[idx].ref_lock);
	memset(&tbl.bufq[idx].krefcount, 0, sizeof(struct kref));
	memset(&tbl.bufq[idx].urefcount, 0, sizeof(struct kref));
	mutex_unlock(&tbl.bufq[idx].ref_lock);
	mutex_destroy(&tbl.bufq[idx].q_lock);
	mutex_destroy(&tbl.bufq[idx].ref_lock);
	clear_bit(idx, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);
}

int cam_mem_get_io_buf(int32_t buf_handle, int32_t mmu_handle,
	dma_addr_t *iova_ptr, size_t *len_ptr, uint32_t *flags)
{
	int i, rc = 0, idx;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM,
			"invalid idx %d for buf handle 0x%x", idx, buf_handle);
		return -ENOENT;
	}
	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		return -EAGAIN;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "buf handle mismatch 0x%x 0x%x mmu handle 0x%x fd %d i_ino %lu",
			buf_handle, tbl.bufq[idx].buf_handle, mmu_handle, tbl.bufq[idx].fd,
			tbl.bufq[idx].i_ino);
		rc = -EINVAL;
		goto handle_mismatch;
	}

	if (!iova_ptr || !len_ptr) {
		CAM_ERR(CAM_MEM, "Error: Input pointers are invalid iova %s len_ptr %s"
			"mmu handle 0x%x fd %d i_ino %lu", CAM_IS_NULL_TO_STR(iova_ptr),
			CAM_IS_NULL_TO_STR(len_ptr), mmu_handle, tbl.bufq[idx].fd,
			tbl.bufq[idx].i_ino);
		rc = -EINVAL;
		goto handle_mismatch;
	}

	/* clean the content from clients */
	*iova_ptr = 0;
	*len_ptr = 0;

	for (i = 0; i < CAM_MEM_MMU_MAX_HANDLE; i++) {
		if (tbl.bufq[idx].hdls[i] == mmu_handle) {
			*iova_ptr = tbl.bufq[idx].iova[i];
			*len_ptr  =  tbl.bufq[idx].len;
			if (flags)
				*flags = tbl.bufq[idx].flags;
			break;
		}
	}

	if (0 == *iova_ptr) {
		CAM_ERR(CAM_MEM,
			"Invalid iova_ptr handle:0x%x fd:%d i_ino:%lu len_ptr:%zu",
			mmu_handle, tbl.bufq[idx].fd, tbl.bufq[idx].i_ino, *len_ptr);
		rc = -EINVAL;
	}

	CAM_DBG(CAM_MEM,
		"handle:0x%x fd:%d i_ino:%lu iova_ptr:0x%llx len_ptr:%llu",
		mmu_handle, tbl.bufq[idx].fd, tbl.bufq[idx].i_ino, *iova_ptr, *len_ptr);

handle_mismatch:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_get_io_buf);

int cam_mem_get_cpu_buf(int32_t buf_handle, uintptr_t *vaddr_ptr, size_t *len)
{
	int idx;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!buf_handle || !vaddr_ptr || !len) {
		CAM_ERR(CAM_MEM, "buf_handle %s vaddr_ptr %s len %s",
				CAM_BOOL_TO_YESNO(buf_handle), CAM_BOOL_TO_YESNO(vaddr_ptr),
				CAM_BOOL_TO_YESNO(len));
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "invalid buf_handle idx %d", idx);
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		return -EPERM;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		return -EINVAL;
	}

	if (!(tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS)) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid flag 0x%x",
					idx, tbl.bufq[idx].flags);
		return -EINVAL;
	}

	mutex_lock(&tbl.bufq[idx].ref_lock);
	if (tbl.bufq[idx].kmdvaddr && kref_get_unless_zero(&tbl.bufq[idx].krefcount)) {
		*vaddr_ptr = tbl.bufq[idx].kmdvaddr;
		*len = tbl.bufq[idx].len;
	} else {
		mutex_unlock(&tbl.bufq[idx].ref_lock);
		CAM_ERR(CAM_MEM, "No KMD access requested, kmdvddr= %p, idx= %d, buf_handle= %d",
			tbl.bufq[idx].kmdvaddr, idx, buf_handle);
		return -EINVAL;
	}
	mutex_unlock(&tbl.bufq[idx].ref_lock);

	return 0;
}
EXPORT_SYMBOL(cam_mem_get_cpu_buf);

int cam_mem_mgr_cache_ops(struct cam_mem_cache_ops_cmd *cmd)
{
	int rc = 0, idx;
	uint32_t cache_dir;
	unsigned long dmabuf_flag = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd)
		return -EINVAL;

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0)
		return -EINVAL;

	mutex_lock(&tbl.m_lock);
	if (!tbl.bitmap) {
		CAM_ERR(CAM_MEM, "crm is closing and mem deinit");
		mutex_unlock(&tbl.m_lock);
		return -EINVAL;
	}
	if (!test_bit(idx, tbl.bitmap)) {
		CAM_ERR(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		mutex_unlock(&tbl.m_lock);
		return -EINVAL;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mutex_unlock(&tbl.m_lock);

	if (cmd->buf_handle != tbl.bufq[idx].buf_handle) {
		rc = -EINVAL;
		goto end;
	}

	rc = dma_buf_get_flags(tbl.bufq[idx].dma_buf, &dmabuf_flag);
	if (rc) {
		CAM_ERR(CAM_MEM, "cache get flags failed %d", rc);
		goto end;
	}

#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)
	CAM_DBG(CAM_MEM, "Calling dmap buf APIs for cache operations");
	cache_dir = DMA_BIDIRECTIONAL;
#else
	if (dmabuf_flag & ION_FLAG_CACHED) {
		switch (cmd->mem_cache_ops) {
		case CAM_MEM_CLEAN_CACHE:
			cache_dir = DMA_TO_DEVICE;
			break;
		case CAM_MEM_INV_CACHE:
			cache_dir = DMA_FROM_DEVICE;
			break;
		case CAM_MEM_CLEAN_INV_CACHE:
			cache_dir = DMA_BIDIRECTIONAL;
			break;
		default:
			CAM_ERR(CAM_MEM,
				"invalid cache ops :%d", cmd->mem_cache_ops);
			rc = -EINVAL;
			goto end;
		}
	} else {
		CAM_DBG(CAM_MEM, "BUF is not cached");
		goto end;
	}
#endif
	rc = dma_buf_begin_cpu_access(tbl.bufq[idx].dma_buf,
		(cmd->mem_cache_ops == CAM_MEM_CLEAN_INV_CACHE) ?
		DMA_BIDIRECTIONAL : DMA_TO_DEVICE);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma begin access failed rc=%d", rc);
		goto end;
	}

	rc = dma_buf_end_cpu_access(tbl.bufq[idx].dma_buf,
		cache_dir);
	if (rc) {
		CAM_ERR(CAM_MEM, "dma end access failed rc=%d", rc);
		goto end;
	}

end:
	mutex_unlock(&tbl.bufq[idx].q_lock);
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_cache_ops);

#if IS_REACHABLE(CONFIG_DMABUF_HEAPS)

#define CAM_MAX_VMIDS 4

static void cam_mem_mgr_put_dma_heaps(void)
{
	CAM_DBG(CAM_MEM, "Releasing DMA Buf heaps usage");
}

static int cam_mem_mgr_get_dma_heaps(void)
{
	int rc = 0;

	tbl.system_heap = NULL;
	tbl.system_uncached_heap = NULL;
	tbl.camera_heap = NULL;
	tbl.camera_uncached_heap = NULL;
	tbl.secure_display_heap = NULL;

	tbl.system_heap = dma_heap_find("qcom,system");
	if (IS_ERR_OR_NULL(tbl.system_heap)) {
		rc = PTR_ERR(tbl.system_heap);
		CAM_ERR(CAM_MEM, "qcom system heap not found, rc=%d", rc);
		tbl.system_heap = NULL;
		goto put_heaps;
	}

	tbl.system_uncached_heap = dma_heap_find("qcom,system-uncached");
	if (IS_ERR_OR_NULL(tbl.system_uncached_heap)) {
		if (tbl.force_cache_allocs) {
			/* optional, we anyway do not use uncached */
			CAM_DBG(CAM_MEM,
				"qcom system-uncached heap not found, err=%d",
				PTR_ERR(tbl.system_uncached_heap));
			tbl.system_uncached_heap = NULL;
		} else {
			/* fatal, must need uncached heaps */
			rc = PTR_ERR(tbl.system_uncached_heap);
			CAM_ERR(CAM_MEM,
				"qcom system-uncached heap not found, rc=%d",
				rc);
			tbl.system_uncached_heap = NULL;
			goto put_heaps;
		}
	}

	tbl.secure_display_heap = dma_heap_find("qcom,display");
	if (IS_ERR_OR_NULL(tbl.secure_display_heap)) {
		CAM_INFO(CAM_MEM, "qcom,display heap not found, rc=%d",
			PTR_ERR(tbl.secure_display_heap));
		tbl.secure_display_heap = NULL;
	}

	tbl.camera_heap = dma_heap_find("qcom,camera");
	if (IS_ERR_OR_NULL(tbl.camera_heap)) {
		/* optional heap, not a fatal error */
		CAM_DBG(CAM_MEM, "qcom camera heap not found, err=%d",
			PTR_ERR(tbl.camera_heap));
		tbl.camera_heap = NULL;
	}

	tbl.camera_uncached_heap = dma_heap_find("qcom,camera-uncached");
	if (IS_ERR_OR_NULL(tbl.camera_uncached_heap)) {
		/* optional heap, not a fatal error */
		CAM_DBG(CAM_MEM, "qcom camera heap not found, err=%d",
			PTR_ERR(tbl.camera_uncached_heap));
		tbl.camera_uncached_heap = NULL;
	}

	CAM_INFO(CAM_MEM,
		"Heaps : system=%pK, system_uncached=%pK, camera=%pK, camera-uncached=%pK, secure_display=%pK",
		tbl.system_heap, tbl.system_uncached_heap,
		tbl.camera_heap, tbl.camera_uncached_heap,
		tbl.secure_display_heap);

	return 0;
put_heaps:
	cam_mem_mgr_put_dma_heaps();
	return rc;
}

static int cam_mem_util_get_dma_buf(size_t len,
	unsigned int cam_flags,
	struct dma_buf **buf,
	unsigned long *i_ino)
{
	int rc = 0;
	struct dma_heap *heap;
	struct dma_heap *try_heap = NULL;
	struct timespec64 ts1, ts2;
	long microsec = 0;
	bool use_cached_heap = false;
	int vmids[CAM_MAX_VMIDS];
	int perms[CAM_MAX_VMIDS];
	int num_vmids = 0;

	if (!buf) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	if (tbl.alloc_profile_enable)
		CAM_GET_TIMESTAMP(ts1);

	if ((cam_flags & CAM_MEM_FLAG_CACHE) ||
		(tbl.force_cache_allocs &&
		(!(cam_flags & CAM_MEM_FLAG_PROTECTED_MODE)))) {
		CAM_DBG(CAM_MEM,
			"Using CACHED heap, cam_flags=0x%x, force_cache_allocs=%d",
			cam_flags, tbl.force_cache_allocs);
		use_cached_heap = true;
	} else if (cam_flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		use_cached_heap = true;
		CAM_DBG(CAM_MEM,
			"Using CACHED heap for secure, cam_flags=0x%x, force_cache_allocs=%d",
			cam_flags, tbl.force_cache_allocs);
	} else {
		use_cached_heap = false;
		CAM_ERR(CAM_MEM,
			"Using UNCACHED heap not supported, cam_flags=0x%x, force_cache_allocs=%d",
			cam_flags, tbl.force_cache_allocs);
		/*
		 * Need a better handling based on whether dma-buf-heaps support
		 * uncached heaps or not. For now, assume not supported.
		 */
		return -EINVAL;
	}

	if (cam_flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		heap = tbl.secure_display_heap;

		vmids[num_vmids] = VMID_CP_CAMERA;
		perms[num_vmids] = PERM_READ | PERM_WRITE;
		num_vmids++;

		if (cam_flags & CAM_MEM_FLAG_CDSP_OUTPUT) {
			CAM_DBG(CAM_MEM, "Secure mode CDSP flags");

			vmids[num_vmids] = VMID_CP_CDSP;
			perms[num_vmids] = PERM_READ | PERM_WRITE;
			num_vmids++;
		}
	} else if (cam_flags & CAM_MEM_FLAG_EVA_NOPIXEL) {
		heap = tbl.secure_display_heap;
		vmids[num_vmids] = VMID_CP_NON_PIXEL;
		perms[num_vmids] = PERM_READ | PERM_WRITE;
		num_vmids++;
	} else if (use_cached_heap) {
		try_heap = tbl.camera_heap;
		heap = tbl.system_heap;
	} else {
		try_heap = tbl.camera_uncached_heap;
		heap = tbl.system_uncached_heap;
	}

	CAM_DBG(CAM_MEM, "Using heaps : try=%pK, heap=%pK", try_heap, heap);

	*buf = NULL;

	if (!try_heap && !heap) {
		CAM_ERR(CAM_MEM,
			"No heap available for allocation, cant allocate");
		return -EINVAL;
	}

	if (try_heap) {
		*buf = dma_heap_buffer_alloc(try_heap, len, O_RDWR, 0);
		if (IS_ERR(*buf)) {
			CAM_WARN(CAM_MEM,
				"Failed in allocating from try heap, heap=%pK, len=%zu, err=%d",
				try_heap, len, PTR_ERR(*buf));
			*buf = NULL;
		}
	}

	if (*buf == NULL) {
		*buf = dma_heap_buffer_alloc(heap, len, O_RDWR, 0);
		if (IS_ERR(*buf)) {
			rc = PTR_ERR(*buf);
			CAM_ERR(CAM_MEM,
				"Failed in allocating from heap, heap=%pK, len=%zu, err=%d",
				heap, len, rc);
			*buf = NULL;
			return rc;
		}
	}

	*i_ino = file_inode((*buf)->file)->i_ino;

#if IS_REACHABLE(CONFIG_QCOM_MEM_BUF)
	if ((cam_flags & CAM_MEM_FLAG_PROTECTED_MODE) ||
		(cam_flags & CAM_MEM_FLAG_EVA_NOPIXEL)) {
		struct mem_buf_lend_kernel_arg arg;
		if (num_vmids >= CAM_MAX_VMIDS) {
			CAM_ERR(CAM_MEM, "Insufficient array size for vmids %d", num_vmids);
			rc = -EINVAL;
			goto end;
		}

		arg.nr_acl_entries = num_vmids;
		arg.vmids = vmids;
		arg.perms = perms;

		rc = mem_buf_lend(*buf, &arg);
		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in buf lend rc=%d, buf=%pK, vmids [0]=0x%x, [1]=0x%x, [2]=0x%x",
				rc, *buf, vmids[0], vmids[1], vmids[2]);
			goto end;
		}
	}
#endif

	CAM_DBG(CAM_MEM, "Allocate success, len=%zu, *buf=%pK, i_ino=%lu", len, *buf, *i_ino);

	if (tbl.alloc_profile_enable) {
		CAM_GET_TIMESTAMP(ts2);
		CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts1, ts2, microsec);
		trace_cam_log_event("IONAllocProfile", "size and time in micro",
			len, microsec);
	}

	return rc;
#if IS_REACHABLE(CONFIG_QCOM_MEM_BUF)
end:
#endif
	dma_buf_put(*buf);
	return rc;
}
#else
static int cam_mem_util_get_dma_buf(size_t len,
	unsigned int cam_flags,
	struct dma_buf **buf,
	unsigned long *i_ino)
{
	int rc = 0;
	unsigned int heap_id;
	int32_t ion_flag = 0;
	struct timespec64 ts1, ts2;
	long microsec = 0;

	if (!buf) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	if (tbl.alloc_profile_enable)
		CAM_GET_TIMESTAMP(ts1);

	if ((cam_flags & CAM_MEM_FLAG_PROTECTED_MODE) &&
		(cam_flags & CAM_MEM_FLAG_CDSP_OUTPUT)) {
		heap_id = ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
		ion_flag |=
			ION_FLAG_SECURE | ION_FLAG_CP_CAMERA | ION_FLAG_CP_CDSP;
	} else if (cam_flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		heap_id = ION_HEAP(ION_SECURE_DISPLAY_HEAP_ID);
		ion_flag |= ION_FLAG_SECURE | ION_FLAG_CP_CAMERA;
	} else {
		heap_id = ION_HEAP(ION_SYSTEM_HEAP_ID) |
			ION_HEAP(ION_CAMERA_HEAP_ID);
	}

	if (cam_flags & CAM_MEM_FLAG_CACHE)
		ion_flag |= ION_FLAG_CACHED;
	else
		ion_flag &= ~ION_FLAG_CACHED;

	if (tbl.force_cache_allocs && (!(ion_flag & ION_FLAG_SECURE)))
		ion_flag |= ION_FLAG_CACHED;

	*buf = ion_alloc(len, heap_id, ion_flag);
	if (IS_ERR_OR_NULL(*buf))
		return -ENOMEM;

	*i_ino = file_inode((*buf)->file)->i_ino;

	if (tbl.alloc_profile_enable) {
		CAM_GET_TIMESTAMP(ts2);
		CAM_GET_TIMESTAMP_DIFF_IN_MICRO(ts1, ts2, microsec);
		trace_cam_log_event("IONAllocProfile", "size and time in micro",
			len, microsec);
	}

	return rc;
}
#endif

static int cam_mem_util_buffer_alloc(size_t len, uint32_t flags,
	struct dma_buf **dmabuf,
	int *fd,
	unsigned long *i_ino)
{
	int rc;

	rc = cam_mem_util_get_dma_buf(len, flags, dmabuf, i_ino);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Error allocating dma buf : len=%llu, flags=0x%x",
			len, flags);
		return rc;
	}

	/*
	 * increment the ref count so that ref count becomes 2 here
	 * when we close fd, refcount becomes 1 and when we do
	 * dmap_put_buf, ref count becomes 0 and memory will be freed.
	 */
	get_dma_buf(*dmabuf);

	*fd = dma_buf_fd(*dmabuf, O_CLOEXEC);
	if (*fd < 0) {
		CAM_ERR(CAM_MEM, "get fd fail, *fd=%d", *fd);
		rc = -EINVAL;
		goto put_buf;
	}

	CAM_DBG(CAM_MEM, "Alloc success : len=%zu, *dmabuf=%pK, fd=%d, i_ino=%lu",
		len, *dmabuf, *fd, *i_ino);

	return rc;

put_buf:
	dma_buf_put(*dmabuf);
	return rc;
}

static int cam_mem_util_check_alloc_flags(struct cam_mem_mgr_alloc_cmd *cmd)
{
	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl exceeded maximum(%d)",
			CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_MEM, "Kernel mapping in secure mode not allowed");
		return -EINVAL;
	}

	if ((cmd->flags & CAM_MEM_FLAG_EVA_NOPIXEL) &&
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE ||
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS)){
		CAM_ERR(CAM_MEM,
			"Kernel mapping and secure mode not allowed in no pixel mode");
		return -EINVAL;
	}

	if ((cmd->flags & CAM_MEM_FLAG_NSP_ACCESS) &&
		(cmd->flags & CAM_MEM_FLAG_KMD_ACCESS))
	{
		CAM_ERR(CAM_MEM,
			"Kernel mapping and with nsp mapping not allowed");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_check_map_flags(struct cam_mem_mgr_map_cmd *cmd)
{
	if (!cmd->flags) {
		CAM_ERR(CAM_MEM, "Invalid flags");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE &&
		cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		CAM_ERR(CAM_MEM,
			"Kernel mapping in secure mode not allowed, flags=0x%x",
			cmd->flags);
		return -EINVAL;
	}

	if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		CAM_ERR(CAM_MEM,
			"Shared memory buffers are not allowed to be mapped");
		return -EINVAL;
	}

	return 0;
}

static int cam_mem_util_map_hw_va(uint32_t flags,
	int32_t *mmu_hdls,
	int32_t num_hdls,
	int fd,
	struct dma_buf *dmabuf,
	size_t *len,
	enum cam_smmu_region_id region,
	dma_addr_t *iova,
	bool is_internal)
{
	int i;
	int rc = -1;
	int dir = cam_mem_util_get_dma_dir(flags);
	bool dis_delayed_unmap = false;

	if (dir < 0) {
		CAM_ERR(CAM_MEM, "fail to map DMA direction, dir=%d", dir);
		return dir;
	}

	if (flags & CAM_MEM_FLAG_DISABLE_DELAYED_UNMAP)
		dis_delayed_unmap = true;

	CAM_DBG(CAM_MEM,
		"map_hw_va : fd = %d, flags = 0x%x, dir=%d, num_hdls=%d",
		fd, flags, dir, num_hdls);

	for (i = 0; i < num_hdls; i++) {
		/* If 36-bit enabled, check for ICP cmd buffers and map them within the shared region */
		if (cam_smmu_is_expanded_memory() &&
			cam_smmu_supports_shared_region(mmu_hdls[i]) &&
			(flags & CAM_MEM_FLAG_CMD_BUF_TYPE))
			region = CAM_SMMU_REGION_SHARED;

		if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
			rc = cam_smmu_map_stage2_iova(mmu_hdls[i], fd, dmabuf,
					dir, &iova[i], len);
		else
			rc = cam_smmu_map_user_iova(mmu_hdls[i], fd, dmabuf, dis_delayed_unmap, dir,
				&iova[i], len, region, is_internal);

		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed %s map to smmu, i=%d, fd=%d, dir=%d, mmu_hdl=%d rc = %d",
				(flags & CAM_MEM_FLAG_PROTECTED_MODE) ? "secured" : "non-secured" ,
				i, fd, dir, mmu_hdls[i], rc);
			goto multi_map_fail;
		}
	}

	return rc;
multi_map_fail:
	for (--i; i>= 0; i--) {
		if (flags & CAM_MEM_FLAG_PROTECTED_MODE)
			cam_smmu_unmap_stage2_iova(mmu_hdls[i], fd, dmabuf);
		else
			cam_smmu_unmap_user_iova(mmu_hdls[i], fd, dmabuf, region);

		iova[i] = 0;
	}

	return rc;
}

static int cam_mem_add_buf_to_eb(int index)
{
	if ((index >= CAM_MEM_BUFQ_MAX) || (index <= 0)) {
		CAM_ERR(CAM_MEM, "index %d", index);
		return -EINVAL;
	}

	mutex_lock(&tbl.eb_tbl.eb_mutex);
	hash_add(tbl.eb_tbl.eb_map, &tbl.bufq[index].hlist, PTR_TO_U64(tbl.bufq[index].dma_buf));
	tbl.bufq[index].bufq_idx = index;
	mutex_unlock(&tbl.eb_tbl.eb_mutex);

	CAM_DBG(CAM_MEM, "existing buf table index %d dmabuf %pK", index, tbl.bufq[index].dma_buf);

	return 0;
}

static void cam_mem_remove_buf_from_eb(int index)
{
	if ((index <= 0) || (index >= CAM_MEM_BUFQ_MAX)) {
		CAM_ERR(CAM_MEM, "Invalid input index %d", index);
		return;
	}

	mutex_lock(&tbl.eb_tbl.eb_mutex);
	hash_del(&tbl.bufq[index].hlist);
	tbl.bufq[index].bufq_idx = 0;
	mutex_unlock(&tbl.eb_tbl.eb_mutex);

	CAM_DBG(CAM_MEM, "remove buf from index %d", index);
}

int cam_mem_mgr_alloc_and_map(struct cam_mem_mgr_alloc_cmd *cmd)
{
	int rc;
	int32_t idx;
	struct dma_buf *dmabuf = NULL;
	int fd = -1;
	size_t len;
	uintptr_t kvaddr = 0;
	size_t klen;
	unsigned long i_ino = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_MEM, " Invalid argument");
		return -EINVAL;
	}

	len = cmd->len;

	if (tbl.need_shared_buffer_padding &&
		(cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)) {
		len += CAM_MEM_SHARED_BUFFER_PAD_4K;
		CAM_DBG(CAM_MEM, "Pad 4k size, actual %llu, allocating %zu",
			cmd->len, len);
	}

	rc = cam_mem_util_check_alloc_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = 0x%X, rc=%d",
			cmd->flags, rc);
		return rc;
	}

	rc = cam_mem_util_buffer_alloc(len, cmd->flags, &dmabuf, &fd, &i_ino);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Ion Alloc failed, len=%llu, align=%llu, flags=0x%x, num_hdl=%d",
			len, cmd->align, cmd->flags, cmd->num_hdl);
		cam_mem_mgr_print_tbl();
		return rc;
	}
	if (!dmabuf) {
		CAM_ERR(CAM_MEM,
			"Ion Alloc return NULL dmabuf! fd=%d, i_ino=%lu, len=%d", fd, i_ino, len);
		cam_mem_mgr_print_tbl();
		return rc;
	}

	idx = cam_mem_get_slot_for_dmabuf(dmabuf, NULL);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d", idx);
		rc = -ENOMEM;
		goto slot_fail;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {

		enum cam_smmu_region_id region;

		if (cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;


		if (cmd->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)
			region = CAM_SMMU_REGION_SHARED;

		if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
			region = CAM_SMMU_REGION_IO;

		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			fd,
			dmabuf,
			&len,
			region,
			tbl.bufq[idx].iova,
			true);
		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va len=%llu, flags=0x%x, fd=%d, region=%d, num_hdl=%d, rc=%d",
				len, cmd->flags,
				fd, region, cmd->num_hdl, rc);
			if (rc == -EALREADY) {
				if ((size_t)dmabuf->size != len)
					rc = -EBADR;
				cam_mem_mgr_print_tbl();
			}
			goto map_hw_fail;
		}
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].fd = fd;
	tbl.bufq[idx].i_ino = i_ino;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, fd);
	tbl.bufq[idx].is_internal = true;
	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
		CAM_MEM_MGR_SET_SECURE_HDL(tbl.bufq[idx].buf_handle, true);

	if (cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		rc = cam_mem_util_map_cpu_va(dmabuf, &kvaddr, &klen);
		if (rc) {
			CAM_ERR(CAM_MEM, "dmabuf: %pK mapping failed: %d",
				dmabuf, rc);
			mutex_unlock(&tbl.bufq[idx].q_lock);
			goto map_kernel_fail;
		}
	}

	if (cmd->flags & CAM_MEM_FLAG_KMD_DEBUG_FLAG)
		tbl.dbg_buf_idx = idx;

	tbl.bufq[idx].kmdvaddr = kvaddr;
	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = len;
	tbl.bufq[idx].num_hdl = cmd->num_hdl;
	cam_mem_mgr_reset_presil_params(idx);
	memcpy(tbl.bufq[idx].hdls, cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);

	tbl.bufq[idx].is_imported = false;

	if (cmd->flags & CAM_MEM_FLAG_KMD_ACCESS)
		kref_init(&tbl.bufq[idx].krefcount);

	kref_init(&tbl.bufq[idx].urefcount);

	tbl.bufq[idx].smmu_mapping_client = CAM_SMMU_MAPPING_USER;

	rc = cam_mem_add_buf_to_eb(idx);
	if (rc < 0) {
		CAM_ERR(CAM_MEM, "add buf to existing buf table failed dma_buf %pK idx %d",
			tbl.bufq[idx].dma_buf, idx);
		mutex_unlock(&tbl.bufq[idx].q_lock);
		goto map_kernel_fail;
	}

	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.fd = tbl.bufq[idx].fd;
	cmd->out.vaddr = 0;

	if (cmd->flags & CAM_MEM_FLAG_NSP_ACCESS)
	{
		rc = cam_fastrpc_dev_map_dma(&tbl.bufq[idx],
			true,
			&cmd->out.vaddr);
		if (rc) {
			CAM_ERR(CAM_MEM, "dmabuf: %pK fastrpc mapping failed: %d",
				dmabuf, rc);
			goto fastrpc_map_fail;
		}
		tbl.bufq[idx].is_nsp_buf = true;
	}

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu, i_ino=%lu",
		cmd->out.fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len, tbl.bufq[idx].i_ino);

	return rc;

fastrpc_map_fail:
	cam_mem_remove_buf_from_eb(idx);
map_kernel_fail:
	cam_mem_mgr_unmap_active_buf(idx);
map_hw_fail:
	cam_mem_put_slot(idx);
slot_fail:
	dma_buf_put(dmabuf);
	return rc;
}

int cam_mem_mgr_map(struct cam_mem_mgr_map_cmd *cmd)
{
	int32_t idx, i, mmu_hdl_idx = 0;
	int rc;
	struct dma_buf *dmabuf;
	size_t len = 0, klen = 0;
	bool is_internal = false;
	bool is_exist = false;
	unsigned long i_ino;
	uintptr_t kvaddr = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd || (cmd->fd < 0)) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	if (cmd->num_hdl > CAM_MEM_MMU_MAX_HANDLE) {
		CAM_ERR(CAM_MEM, "Num of mmu hdl %d exceeded maximum(%d)",
			cmd->num_hdl, CAM_MEM_MMU_MAX_HANDLE);
		return -EINVAL;
	}

	rc = cam_mem_util_check_map_flags(cmd);
	if (rc) {
		CAM_ERR(CAM_MEM, "Invalid flags: flags = %X", cmd->flags);
		return rc;
	}

	dmabuf = dma_buf_get(cmd->fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		CAM_ERR(CAM_MEM, "Failed to import dma_buf fd %d, err %d",
			cmd->fd, PTR_ERR(dmabuf));
		return -EINVAL;
	}

	i_ino = file_inode(dmabuf->file)->i_ino;

	idx = cam_mem_get_slot_for_dmabuf(dmabuf, &is_exist);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d, fd=%d", idx, cmd->fd);
		rc = -ENOMEM;
		goto slot_fail;
	}

	is_internal = tbl.bufq[idx].is_internal;

	if (is_exist) {
		CAM_DBG(CAM_MEM,
			"buf map existed in queue, use the same slot index=%d fd=%d flags=0x%x num_hdl=%d buf handle=%x, len=%zu, i_ino=%lu is_internal=%d",
			idx, cmd->fd, tbl.bufq[idx].flags, tbl.bufq[idx].num_hdl,
			tbl.bufq[idx].buf_handle, tbl.bufq[idx].len, tbl.bufq[idx].i_ino,
			is_internal);
		for (i = 0; i < tbl.bufq[idx].num_hdl; i++)
			CAM_DBG(CAM_MEM, "index %d mmu handle 0x%x iova_ptr:0x%lx",
				i, tbl.bufq[idx].hdls[i], tbl.bufq[idx].iova[i]);

		mmu_hdl_idx = tbl.bufq[idx].num_hdl;
	}

	if ((cmd->flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)) {

		rc = cam_mem_util_map_hw_va(cmd->flags,
			cmd->mmu_hdls,
			cmd->num_hdl,
			cmd->fd,
			dmabuf,
			&len,
			CAM_SMMU_REGION_IO,
			&tbl.bufq[idx].iova[mmu_hdl_idx],
			is_internal);
		if (rc) {
			CAM_ERR(CAM_MEM,
				"Failed in map_hw_va, flags=0x%x, fd=%d, len=%llu, region=%d, num_hdl=%d, rc=%d",
				cmd->flags, cmd->fd, len,
				CAM_SMMU_REGION_IO, cmd->num_hdl, rc);
			if (rc == -EALREADY) {
				if ((size_t)dmabuf->size != len) {
					rc = -EBADR;
					cam_mem_mgr_print_tbl();
				}
			}
			goto map_fail;
		}
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	if (cmd->flags & CAM_MEM_FLAG_NSP_ACCESS) {
		cmd->flags = (cmd->flags & ~CAM_MEM_FLAG_NSP_ACCESS);
		tbl.bufq[idx].is_nsp_buf = true;
	}

	tbl.bufq[idx].fd = cmd->fd;
	tbl.bufq[idx].i_ino = i_ino;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].flags = cmd->flags;
	tbl.bufq[idx].buf_handle = GET_MEM_HANDLE(idx, cmd->fd);
	if (cmd->flags & CAM_MEM_FLAG_PROTECTED_MODE)
		CAM_MEM_MGR_SET_SECURE_HDL(tbl.bufq[idx].buf_handle, true);

	if (!tbl.bufq[idx].kmdvaddr && cmd->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		rc = cam_mem_util_map_cpu_va(dmabuf, &kvaddr, &klen);
		if (rc) {
			tbl.bufq[idx].kmdvaddr = 0;
			CAM_ERR(CAM_MEM, "dmabuf: %pK mapping failed: %d",
					dmabuf, rc);
			goto map_kernel_fail;
		}
	}

	tbl.bufq[idx].kmdvaddr = kvaddr;
	tbl.bufq[idx].dma_buf = dmabuf;
	tbl.bufq[idx].len = len;
	tbl.bufq[idx].num_hdl = mmu_hdl_idx + cmd->num_hdl;
	memcpy(&tbl.bufq[idx].hdls[mmu_hdl_idx], cmd->mmu_hdls,
		sizeof(int32_t) * cmd->num_hdl);

	tbl.bufq[idx].is_imported = true;
	tbl.bufq[idx].is_internal = is_internal;

	if (!is_exist) {
		if (cmd->flags & CAM_MEM_FLAG_KMD_ACCESS)
			kref_init(&tbl.bufq[idx].krefcount);
		kref_init(&tbl.bufq[idx].urefcount);
	} else {
		kref_get(&tbl.bufq[idx].urefcount);
		dma_buf_put(dmabuf);
	}

	tbl.bufq[idx].smmu_mapping_client = CAM_SMMU_MAPPING_USER;

	if (!is_exist) {
		rc = cam_mem_add_buf_to_eb(idx);
		if (rc < 0) {
			CAM_ERR(CAM_MEM, "add buf to existing buf table failed dma_buf %pK idx %d",
				tbl.bufq[idx].dma_buf, idx);
			goto map_kernel_fail;
		}
	}

	mutex_unlock(&tbl.bufq[idx].q_lock);

	cmd->out.buf_handle = tbl.bufq[idx].buf_handle;
	cmd->out.vaddr = 0;
	cmd->out.size = (uint32_t)len;

	CAM_DBG(CAM_MEM,
		"fd=%d, flags=0x%x, num_hdl=%d, idx=%d, buf handle=%x, len=%zu, i_ino=%lu",
		cmd->fd, cmd->flags, cmd->num_hdl, idx, cmd->out.buf_handle,
		tbl.bufq[idx].len, tbl.bufq[idx].i_ino);

	return rc;
map_kernel_fail:
	mutex_unlock(&tbl.bufq[idx].q_lock);
map_fail:
	cam_mem_put_slot(idx);
slot_fail:
	dma_buf_put(dmabuf);
	return rc;
}

static int cam_mem_util_unmap_hw_va(int32_t idx,
	enum cam_smmu_region_id region,
	enum cam_smmu_mapping_client client)
{
	int i;
	uint32_t flags;
	int32_t *mmu_hdls;
	int num_hdls;
	int fd;
	struct dma_buf *dma_buf;
	unsigned long i_ino;
	int rc = 0;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return -EINVAL;
	}

	flags = tbl.bufq[idx].flags;
	mmu_hdls = tbl.bufq[idx].hdls;
	num_hdls = tbl.bufq[idx].num_hdl;
	fd = tbl.bufq[idx].fd;
	dma_buf = tbl.bufq[idx].dma_buf;
	i_ino = tbl.bufq[idx].i_ino;

	CAM_DBG(CAM_MEM,
		"unmap_hw_va : idx=%d, fd=%x, i_ino=%lu flags=0x%x, num_hdls=%d, client=%d",
		idx, fd, i_ino, flags, num_hdls, client);

	if (flags & CAM_MEM_FLAG_PROTECTED_MODE) {
		for (i = 0; i < num_hdls; i++) {
			rc = cam_smmu_unmap_stage2_iova(mmu_hdls[i], fd, dma_buf);
			if (rc < 0) {
				CAM_ERR(CAM_MEM,
					"Failed in secure unmap, i=%d, fd=%d, i_ino=%lu, mmu_hdl=%d, rc=%d",
					i, fd, i_ino, mmu_hdls[i], rc);
				goto unmap_end;
			}
		}
	} else {
		for (i = 0; i < num_hdls; i++) {
			if (client == CAM_SMMU_MAPPING_USER) {
				rc = cam_smmu_unmap_user_iova(mmu_hdls[i],
					fd, dma_buf, region);
			} else if (client == CAM_SMMU_MAPPING_KERNEL) {
				rc = cam_smmu_unmap_kernel_iova(mmu_hdls[i],
					tbl.bufq[idx].dma_buf, region);
			} else {
				CAM_ERR(CAM_MEM,
					"invalid caller for unmapping : %d",
					client);
				rc = -EINVAL;
			}
			if (rc < 0) {
				CAM_ERR(CAM_MEM,
					"Failed in unmap, i=%d, fd=%d, i_ino=%lu, mmu_hdl=%d, region=%d, rc=%d",
					i, fd, i_ino, mmu_hdls[i], region, rc);
				goto unmap_end;
			}
		}
	}

	return rc;

unmap_end:
	CAM_ERR(CAM_MEM, "unmapping failed");
	return rc;
}

static void cam_mem_mgr_unmap_active_buf(int idx)
{
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS)
		region = CAM_SMMU_REGION_SHARED;
	else if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
		region = CAM_SMMU_REGION_IO;

	cam_mem_util_unmap_hw_va(idx, region, CAM_SMMU_MAPPING_USER);

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		cam_mem_util_unmap_cpu_va(tbl.bufq[idx].dma_buf,
			tbl.bufq[idx].kmdvaddr);
		tbl.bufq[idx].kmdvaddr = 0;
	}
}

static int cam_mem_mgr_cleanup_table(void)
{
	int i;

	mutex_lock(&tbl.m_lock);
	for (i = 1; i < CAM_MEM_BUFQ_MAX; i++) {
		if (!tbl.bufq[i].active) {
			CAM_DBG(CAM_MEM,
				"Buffer inactive at idx=%d, continuing", i);
			continue;
		} else {
			CAM_WARN(CAM_MEM,
			"Active buffer idx=%d TS=%dus len=%lld flags=0x%x fd=%d, i_ino=%lu int/imp %d/%d , possible leak needs unmapping",
			i, tbl.bufq[i].timestamp.tv_sec * 1000000 + tbl.bufq[i].timestamp.tv_nsec/1000,
			tbl.bufq[i].len, tbl.bufq[i].flags,
			tbl.bufq[i].fd, tbl.bufq[i].i_ino,
			tbl.bufq[i].is_internal, tbl.bufq[i].is_imported);

			cam_mem_mgr_unmap_active_buf(i);
		}

		mutex_lock(&tbl.bufq[i].q_lock);
		if (tbl.bufq[i].dma_buf) {
			dma_buf_put(tbl.bufq[i].dma_buf);
			tbl.bufq[i].dma_buf = NULL;
		}

		cam_mem_remove_buf_from_eb(i);

		tbl.bufq[i].fd = -1;
		tbl.bufq[i].i_ino = 0;
		tbl.bufq[i].flags = 0;
		tbl.bufq[i].buf_handle = -1;
		tbl.bufq[i].len = 0;
		memset(tbl.bufq[i].hdls, 0,
			sizeof(int32_t) * tbl.bufq[i].num_hdl);
		memset(tbl.bufq[i].iova, 0,
			sizeof(dma_addr_t) * CAM_MEM_MMU_MAX_HANDLE);
		tbl.bufq[i].num_hdl = 0;
		tbl.bufq[i].dma_buf = NULL;
		tbl.bufq[i].active = false;
		tbl.bufq[i].is_internal = false;
		cam_mem_mgr_reset_presil_params(i);
		mutex_unlock(&tbl.bufq[i].q_lock);
		mutex_lock(&tbl.bufq[i].ref_lock);
		memset(&tbl.bufq[i].krefcount, 0, sizeof(struct kref));
		memset(&tbl.bufq[i].urefcount, 0, sizeof(struct kref));
		mutex_unlock(&tbl.bufq[i].ref_lock);
		mutex_destroy(&tbl.bufq[i].q_lock);
		mutex_destroy(&tbl.bufq[i].ref_lock);
	}

	bitmap_zero(tbl.bitmap, tbl.bits);
	/* We need to reserve slot 0 because 0 is invalid */
	set_bit(0, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);

	return 0;
}

void cam_mem_mgr_deinit(void)
{
	atomic_set(&cam_mem_mgr_state, CAM_MEM_MGR_UNINITIALIZED);
	cam_mem_mgr_cleanup_table();
	debugfs_remove_recursive(tbl.dentry);
	mutex_lock(&tbl.m_lock);
	bitmap_zero(tbl.bitmap, tbl.bits);
	kfree(tbl.bitmap);
	tbl.bitmap = NULL;
	tbl.dbg_buf_idx = -1;
	mutex_unlock(&tbl.m_lock);
	mutex_destroy(&tbl.m_lock);
}

static void cam_mem_util_unmap_dummy(struct kref *kref)
{
	CAM_DBG(CAM_MEM, "Cam mem util unmap dummy");
}

static void cam_mem_util_unmap(int32_t idx)
{
	int rc = 0;
	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;
	enum cam_smmu_mapping_client client;

	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index");
		return;
	}

	client = tbl.bufq[idx].smmu_mapping_client;

	CAM_DBG(CAM_MEM, "Flags = %X idx %d", tbl.bufq[idx].flags, idx);

	mutex_lock(&tbl.m_lock);
	if ((!tbl.bufq[idx].active) &&
		(tbl.bufq[idx].iova[0]) == 0) {
		CAM_WARN(CAM_MEM, "Buffer at idx=%d is already unmapped,",
			idx);
		mutex_unlock(&tbl.m_lock);
		return;
	}

	/* Deactivate the buffer queue to prevent multiple unmap */
	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].active = false;
	mutex_unlock(&tbl.bufq[idx].q_lock);
	mutex_unlock(&tbl.m_lock);

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		if (tbl.bufq[idx].dma_buf && tbl.bufq[idx].kmdvaddr) {
			rc = cam_mem_util_unmap_cpu_va(tbl.bufq[idx].dma_buf,
				tbl.bufq[idx].kmdvaddr);
			if (rc)
				CAM_ERR(CAM_MEM,
					"Failed, dmabuf=%pK, kmdvaddr=%pK",
					tbl.bufq[idx].dma_buf,
					(void *) tbl.bufq[idx].kmdvaddr);
		}
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	if ((tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_READ_WRITE) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) ||
		(tbl.bufq[idx].flags & CAM_MEM_FLAG_PROTECTED_MODE)) {
		if (cam_mem_util_unmap_hw_va(idx, region, client))
			CAM_ERR(CAM_MEM, "Failed, dmabuf=%pK",
				tbl.bufq[idx].dma_buf);
	}
	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_NSP_ACCESS)
	{
		rc = cam_fastrpc_dev_unmap_dma(&tbl.bufq[idx]);
	}

	mutex_lock(&tbl.m_lock);
	mutex_lock(&tbl.bufq[idx].q_lock);
	tbl.bufq[idx].flags = 0;
	tbl.bufq[idx].buf_handle = -1;
	memset(tbl.bufq[idx].hdls, 0,
		sizeof(int32_t) * CAM_MEM_MMU_MAX_HANDLE);
	memset(tbl.bufq[idx].iova, 0,
		sizeof(dma_addr_t) * CAM_MEM_MMU_MAX_HANDLE);

	CAM_DBG(CAM_MEM,
		"Ion buf at idx = %d freeing fd = %d, imported %d, dma_buf %pK, i_ino %lu",
		idx, tbl.bufq[idx].fd, tbl.bufq[idx].is_imported, tbl.bufq[idx].dma_buf,
		tbl.bufq[idx].i_ino);

	if (tbl.bufq[idx].dma_buf)
		dma_buf_put(tbl.bufq[idx].dma_buf);

	cam_mem_remove_buf_from_eb(idx);

	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].i_ino = 0;
	tbl.bufq[idx].dma_buf = NULL;
	tbl.bufq[idx].is_imported = false;
	tbl.bufq[idx].is_internal = false;
	tbl.bufq[idx].is_nsp_buf = false;
	tbl.bufq[idx].len = 0;
	tbl.bufq[idx].num_hdl = 0;
	tbl.bufq[idx].kmdvaddr = 0;
	cam_mem_mgr_reset_presil_params(idx);
	memset(&tbl.bufq[idx].timestamp, 0, sizeof(struct timespec64));
	memset(&tbl.bufq[idx].krefcount, 0, sizeof(struct kref));
	memset(&tbl.bufq[idx].urefcount, 0, sizeof(struct kref));
	mutex_unlock(&tbl.bufq[idx].q_lock);
	mutex_destroy(&tbl.bufq[idx].q_lock);
	if (!tbl.bitmap) {
		CAM_ERR(CAM_MEM, "crm is closing and mem deinit");
		mutex_unlock(&tbl.m_lock);
		return;
	}
	clear_bit(idx, tbl.bitmap);
	mutex_unlock(&tbl.m_lock);

}

static void cam_mem_util_unmap_wrapper(struct kref *kref)
{
	int32_t idx;
	struct cam_mem_buf_queue *bufq = container_of(kref, typeof(*bufq), krefcount);

	idx = CAM_MEM_MGR_GET_HDL_IDX(bufq->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "idx: %d not valid", idx);
		return;
	}

	cam_mem_util_unmap(idx);

	mutex_destroy(&tbl.bufq[idx].ref_lock);
}

void cam_mem_put_cpu_buf(int32_t buf_handle)
{
	int rc = 0;
	int idx;
	uint32_t krefcount = 0, urefcount = 0;
	bool unmap = false;

	if (!buf_handle) {
		CAM_ERR(CAM_MEM, "Invalid buf_handle");
		return;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "idx: %d not valid", idx);
		return;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "idx: %d not active", idx);
		rc = -EPERM;
		return;
	}

	if (buf_handle != tbl.bufq[idx].buf_handle) {
		CAM_ERR(CAM_MEM, "idx: %d Invalid buf handle %d",
				idx, buf_handle);
		rc = -EINVAL;
		return;
	}

	mutex_lock(&tbl.bufq[idx].ref_lock);
	kref_put(&tbl.bufq[idx].krefcount, cam_mem_util_unmap_dummy);

	krefcount = kref_read(&tbl.bufq[idx].krefcount);
	urefcount = kref_read(&tbl.bufq[idx].urefcount);

	if ((krefcount == 1) && (urefcount == 0))
		unmap = true;

	if (unmap) {
		cam_mem_util_unmap(idx);
		CAM_DBG(CAM_MEM,
			"Called unmap from here, buf_handle: %u, idx: %d", buf_handle, idx);
	} else if (krefcount == 0) {
		CAM_ERR(CAM_MEM,
			"Unbalanced release Called buf_handle: %u, idx: %d",
			tbl.bufq[idx].buf_handle, idx);
	}
	mutex_unlock(&tbl.bufq[idx].ref_lock);

	if (unmap)
		mutex_destroy(&tbl.bufq[idx].ref_lock);

}
EXPORT_SYMBOL(cam_mem_put_cpu_buf);


int cam_mem_mgr_release(struct cam_mem_mgr_release_cmd *cmd)
{
	int idx;
	int rc = 0;
	uint32_t krefcount = 0, urefcount = 0;
	bool unmap = false;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!cmd) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(cmd->buf_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index %d extracted from mem handle 0x%x",
			idx, cmd->buf_handle);
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		CAM_ERR(CAM_MEM, "Released buffer state should be active idx %d", idx);
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != cmd->buf_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle %d not matching within table %d, idx=%d",
			cmd->buf_handle, tbl.bufq[idx].buf_handle, idx);
		return -EINVAL;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %x, idx = %d", cmd->buf_handle, idx);

	mutex_lock(&tbl.bufq[idx].ref_lock);
	kref_put(&tbl.bufq[idx].urefcount, cam_mem_util_unmap_dummy);

	urefcount = kref_read(&tbl.bufq[idx].urefcount);

	if (tbl.bufq[idx].flags & CAM_MEM_FLAG_KMD_ACCESS) {
		krefcount = kref_read(&tbl.bufq[idx].krefcount);
		if ((krefcount == 1) && (urefcount == 0))
			unmap = true;
	} else {
		if (urefcount == 0)
			unmap = true;
	}

	if (unmap) {
		cam_mem_util_unmap(idx);
		CAM_DBG(CAM_MEM,
			"Called unmap from here, buf_handle: %u, idx: %d", cmd->buf_handle, idx);
	}

	mutex_unlock(&tbl.bufq[idx].ref_lock);

	if (unmap)
		mutex_destroy(&tbl.bufq[idx].ref_lock);

	return rc;
}

int cam_mem_mgr_request_mem(struct cam_mem_mgr_request_desc *inp,
	struct cam_mem_mgr_memory_desc *out)
{
	struct dma_buf *buf = NULL;
	int ion_fd = -1;
	int rc = 0;
	uintptr_t kvaddr;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;
	unsigned long i_ino = 0;
	dma_addr_t iova = 0;

	enum cam_smmu_region_id region = CAM_SMMU_REGION_SHARED;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp || !out) {
		CAM_ERR(CAM_MEM, "Invalid params");
		return -EINVAL;
	}

	if (!(inp->flags & CAM_MEM_FLAG_HW_READ_WRITE ||
		inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS ||
		inp->flags & CAM_MEM_FLAG_CACHE)) {
		CAM_ERR(CAM_MEM, "Invalid flags for request mem");
		return -EINVAL;
	}

	rc = cam_mem_util_get_dma_buf(inp->size, inp->flags, &buf, &i_ino);

	if (rc) {
		CAM_ERR(CAM_MEM, "ION alloc failed for shared buffer");
		goto ion_fail;
	} else if (!buf) {
		CAM_ERR(CAM_MEM, "ION alloc returned NULL buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_MEM, "Got dma_buf = %pK", buf);
	}

	/*
	 * we are mapping kva always here,
	 * update flags so that we do unmap properly
	 */
	inp->flags |= CAM_MEM_FLAG_KMD_ACCESS;
	rc = cam_mem_util_map_cpu_va(buf, &kvaddr, &request_len);
	if (rc) {
		CAM_ERR(CAM_MEM, "Failed to get kernel vaddr");
		goto map_fail;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_MEM, "Invalid SMMU handle");
		rc = -EINVAL;
		goto smmu_fail;
	}

	/* SHARED flag gets precedence, all other flags after it */
	if (inp->flags & CAM_MEM_FLAG_HW_SHARED_ACCESS) {
		region = CAM_SMMU_REGION_SHARED;
	} else {
		if (inp->flags & CAM_MEM_FLAG_HW_READ_WRITE)
			region = CAM_SMMU_REGION_IO;
	}

	rc = cam_smmu_map_kernel_iova(inp->smmu_hdl,
		buf,
		CAM_SMMU_MAP_RW,
		&iova,
		&request_len,
		region);
	if (rc < 0) {
		CAM_ERR(CAM_MEM, "SMMU mapping failed");
		goto smmu_fail;
	}

	smmu_hdl = inp->smmu_hdl;
	num_hdl = 1;

	idx = cam_mem_get_slot_for_dmabuf(buf, NULL);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d", idx);
		rc = -ENOMEM;
		goto slot_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mem_handle = GET_MEM_HANDLE(idx, ion_fd);
	tbl.bufq[idx].dma_buf = buf;
	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].i_ino = i_ino;
	tbl.bufq[idx].flags = inp->flags;
	tbl.bufq[idx].buf_handle = mem_handle;
	tbl.bufq[idx].kmdvaddr = kvaddr;

	tbl.bufq[idx].len = inp->size;
	tbl.bufq[idx].num_hdl = num_hdl;
	memcpy(tbl.bufq[idx].hdls, &smmu_hdl,
		sizeof(int32_t));
	tbl.bufq[idx].iova[0] = iova;
	tbl.bufq[idx].is_imported = false;
	kref_init(&tbl.bufq[idx].krefcount);
	tbl.bufq[idx].smmu_mapping_client = CAM_SMMU_MAPPING_KERNEL;

	rc = cam_mem_add_buf_to_eb(idx);
	if (rc < 0) {
		CAM_ERR(CAM_MEM, "add buf to existing buf table failed dma_buf %pK idx %d",
			tbl.bufq[idx].dma_buf, idx);
		mutex_unlock(&tbl.bufq[idx].q_lock);
		goto add_eb_fail;
	}
	mutex_unlock(&tbl.bufq[idx].q_lock);

	out->kva = kvaddr;
	out->iova = (uint32_t)iova;
	out->smmu_hdl = smmu_hdl;
	out->mem_handle = mem_handle;
	out->len = inp->size;
	out->region = region;

	CAM_DBG(CAM_MEM, "idx=%d, dmabuf=%pK, i_ino=%lu, flags=0x%x, mem_handle=0x%x",
		idx, buf, i_ino, inp->flags, mem_handle);

	return rc;

add_eb_fail:
	cam_mem_put_slot(idx);
slot_fail:
	cam_smmu_unmap_kernel_iova(inp->smmu_hdl,
		buf, region);
smmu_fail:
	cam_mem_util_unmap_cpu_va(buf, kvaddr);
map_fail:
	dma_buf_put(buf);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_request_mem);

int cam_mem_mgr_release_mem(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].iova[0] == 0) {
			CAM_ERR(CAM_MEM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %X", inp->mem_handle);
	if (kref_put(&tbl.bufq[idx].krefcount, cam_mem_util_unmap_wrapper))
		CAM_DBG(CAM_MEM,
			"Called unmap from here, buf_handle: %u, idx: %d",
			tbl.bufq[idx].buf_handle, idx);
	else
		rc = -EINVAL;

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_release_mem);

int cam_mem_mgr_reserve_memory_region(struct cam_mem_mgr_request_desc *inp,
	enum cam_smmu_region_id region,
	struct cam_mem_mgr_memory_desc *out)
{
	struct dma_buf *buf = NULL;
	int rc = 0;
	int ion_fd = -1;
	dma_addr_t iova = 0;
	size_t request_len = 0;
	uint32_t mem_handle;
	int32_t idx;
	int32_t smmu_hdl = 0;
	int32_t num_hdl = 0;
	uintptr_t kvaddr = 0;
	unsigned long i_ino = 0;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp || !out) {
		CAM_ERR(CAM_MEM, "Invalid param(s)");
		return -EINVAL;
	}

	if (!inp->smmu_hdl) {
		CAM_ERR(CAM_MEM, "Invalid SMMU handle");
		return -EINVAL;
	}

	if ((region != CAM_SMMU_REGION_SECHEAP) &&
		(region != CAM_SMMU_REGION_FWUNCACHED)) {
		CAM_ERR(CAM_MEM, "Only secondary heap supported");
		return -EINVAL;
	}

	rc = cam_mem_util_get_dma_buf(inp->size, 0, &buf, &i_ino);

	if (rc) {
		CAM_ERR(CAM_MEM, "ION alloc failed for sec heap buffer");
		goto ion_fail;
	} else if (!buf) {
		CAM_ERR(CAM_MEM, "ION alloc returned NULL buffer");
		goto ion_fail;
	} else {
		CAM_DBG(CAM_MEM, "Got dma_buf = %pK", buf);
	}

	if (inp->flags & CAM_MEM_FLAG_KMD_ACCESS) {
		rc = cam_mem_util_map_cpu_va(buf, &kvaddr, &request_len);
		if (rc) {
			CAM_ERR(CAM_MEM, "Failed to get kernel vaddr");
			goto kmap_fail;
		}
	}

	rc = cam_smmu_reserve_buf_region(region,
		inp->smmu_hdl, buf, &iova, &request_len);
	if (rc) {
		CAM_ERR(CAM_MEM, "Reserving secondary heap failed");
		goto smmu_fail;
	}

	smmu_hdl = inp->smmu_hdl;
	num_hdl = 1;

	idx = cam_mem_get_slot_for_dmabuf(buf, NULL);
	if (idx < 0) {
		CAM_ERR(CAM_MEM, "Failed in getting mem slot, idx=%d", idx);
		rc = -ENOMEM;
		goto slot_fail;
	}

	mutex_lock(&tbl.bufq[idx].q_lock);
	mem_handle = GET_MEM_HANDLE(idx, ion_fd);
	tbl.bufq[idx].fd = -1;
	tbl.bufq[idx].i_ino = i_ino;
	tbl.bufq[idx].dma_buf = buf;
	tbl.bufq[idx].flags = inp->flags;
	tbl.bufq[idx].buf_handle = mem_handle;
	tbl.bufq[idx].kmdvaddr = kvaddr;

	tbl.bufq[idx].len = request_len;
	tbl.bufq[idx].num_hdl = num_hdl;
	memcpy(tbl.bufq[idx].hdls, &smmu_hdl,
		sizeof(int32_t));
	tbl.bufq[idx].iova[0] = iova;
	tbl.bufq[idx].is_imported = false;
	kref_init(&tbl.bufq[idx].krefcount);
	tbl.bufq[idx].smmu_mapping_client = CAM_SMMU_MAPPING_KERNEL;

	rc = cam_mem_add_buf_to_eb(idx);
	if (rc < 0) {
		CAM_ERR(CAM_MEM, "add buf to existing buf table failed dma_buf %pK idx %d",
			tbl.bufq[idx].dma_buf, idx);
		mutex_unlock(&tbl.bufq[idx].q_lock);
		goto add_eb_fail;
	}
	mutex_unlock(&tbl.bufq[idx].q_lock);

	out->kva = kvaddr;
	out->iova = (uint32_t)iova;
	out->smmu_hdl = smmu_hdl;
	out->mem_handle = mem_handle;
	out->len = request_len;
	out->region = region;

	return rc;

add_eb_fail:
	cam_mem_put_slot(idx);
slot_fail:
	cam_smmu_release_buf_region(region, smmu_hdl);
smmu_fail:
	if (region == CAM_SMMU_REGION_FWUNCACHED)
		cam_mem_util_unmap_cpu_va(buf, kvaddr);
kmap_fail:
	dma_buf_put(buf);
ion_fail:
	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_reserve_memory_region);

int cam_mem_mgr_free_memory_region(struct cam_mem_mgr_memory_desc *inp)
{
	int32_t idx;
	int rc;
	int32_t smmu_hdl;

	if (!atomic_read(&cam_mem_mgr_state)) {
		CAM_ERR(CAM_MEM, "failed. mem_mgr not initialized");
		return -EINVAL;
	}

	if (!inp) {
		CAM_ERR(CAM_MEM, "Invalid argument");
		return -EINVAL;
	}

	if ((inp->region != CAM_SMMU_REGION_SECHEAP) &&
		(inp->region != CAM_SMMU_REGION_FWUNCACHED)) {
		CAM_ERR(CAM_MEM, "Only secondary heap supported");
		return -EINVAL;
	}

	idx = CAM_MEM_MGR_GET_HDL_IDX(inp->mem_handle);
	if (idx >= CAM_MEM_BUFQ_MAX || idx <= 0) {
		CAM_ERR(CAM_MEM, "Incorrect index extracted from mem handle");
		return -EINVAL;
	}

	if (!tbl.bufq[idx].active) {
		if (tbl.bufq[idx].iova[0] == 0) {
			CAM_ERR(CAM_MEM, "buffer is released already");
			return 0;
		}
		CAM_ERR(CAM_MEM, "Released buffer state should be active");
		return -EINVAL;
	}

	if (tbl.bufq[idx].buf_handle != inp->mem_handle) {
		CAM_ERR(CAM_MEM,
			"Released buf handle not matching within table");
		return -EINVAL;
	}

	if (tbl.bufq[idx].num_hdl != 1) {
		CAM_ERR(CAM_MEM,
			"Sec heap region should have only one smmu hdl");
		return -ENODEV;
	}

	memcpy(&smmu_hdl, tbl.bufq[idx].hdls,
		sizeof(int32_t));
	if (inp->smmu_hdl != smmu_hdl) {
		CAM_ERR(CAM_MEM,
			"Passed SMMU handle doesn't match with internal hdl");
		return -ENODEV;
	}

	rc = cam_smmu_release_buf_region(inp->region, inp->smmu_hdl);
	if (rc) {
		CAM_ERR(CAM_MEM,
			"Sec heap region release failed");
		return -ENODEV;
	}

	CAM_DBG(CAM_MEM, "Releasing hdl = %X", inp->mem_handle);
	if (kref_put(&tbl.bufq[idx].krefcount, cam_mem_util_unmap_wrapper))
		CAM_DBG(CAM_MEM,
			"Called unmap from here, buf_handle: %u, idx: %d",
			inp->mem_handle, idx);
	else
		rc = -EINVAL;

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_free_memory_region);

#ifdef CONFIG_CAM_PRESIL
struct dma_buf *cam_mem_mgr_get_dma_buf(int fd)
{
	struct dma_buf *dmabuf = NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL((void *)(dmabuf))) {
		CAM_ERR(CAM_MEM, "Failed to import dma_buf for fd");
		return NULL;
	}

	CAM_INFO(CAM_PRESIL, "Received DMA Buf* %pK", dmabuf);

	return dmabuf;
}

int cam_presil_put_dmabuf_from_fd(uint64_t input_dmabuf)
{
	struct dma_buf *dmabuf = (struct dma_buf *)(uint64_t)input_dmabuf;
	int idx = 0;

	CAM_INFO(CAM_PRESIL, "Received dma_buf :%pK", dmabuf);

	if (!dmabuf) {
		CAM_ERR(CAM_PRESIL, "NULL to import dma_buf fd");
		return -EINVAL;
	}

	for (idx = 0; idx < CAM_MEM_BUFQ_MAX; idx++) {
		if ((tbl.bufq[idx].dma_buf != NULL) && (tbl.bufq[idx].dma_buf == dmabuf)) {
			if (tbl.bufq[idx].presil_params.refcount)
				tbl.bufq[idx].presil_params.refcount--;
			else
				CAM_ERR(CAM_PRESIL, "Unbalanced dmabuf put: %pK", dmabuf);

			if (!tbl.bufq[idx].presil_params.refcount) {
				dma_buf_put(dmabuf);
				cam_mem_mgr_reset_presil_params(idx);
				CAM_DBG(CAM_PRESIL, "Done dma_buf_put for %pK", dmabuf);
			}
		}
	}

	return 0;
}
EXPORT_SYMBOL(cam_presil_put_dmabuf_from_fd);

int cam_presil_get_fd_from_dmabuf(uint64_t input_dmabuf)
{
	int fd_for_dmabuf = -1;
	struct dma_buf *dmabuf = (struct dma_buf *)(uint64_t)input_dmabuf;
	int idx = 0;

	CAM_DBG(CAM_PRESIL, "Received dma_buf :%pK", dmabuf);

	if (!dmabuf) {
		CAM_ERR(CAM_PRESIL, "NULL to import dma_buf fd");
		return -EINVAL;
	}

	for (idx = 0; idx < CAM_MEM_BUFQ_MAX; idx++) {
		if ((tbl.bufq[idx].dma_buf != NULL) && (tbl.bufq[idx].dma_buf == dmabuf)) {
			CAM_DBG(CAM_PRESIL,
				"Found entry for request from Presil UMD Daemon at %d, dmabuf %pK fd_for_umd_daemon %d refcount: %d",
				idx, tbl.bufq[idx].dma_buf,
				tbl.bufq[idx].presil_params.fd_for_umd_daemon,
				tbl.bufq[idx].presil_params.refcount);

			if (tbl.bufq[idx].presil_params.fd_for_umd_daemon < 0) {
				fd_for_dmabuf = dma_buf_fd(dmabuf, O_CLOEXEC);
				if (fd_for_dmabuf < 0) {
					CAM_ERR(CAM_PRESIL, "get fd fail, fd_for_dmabuf=%d",
						fd_for_dmabuf);
					return -EINVAL;
				}

				tbl.bufq[idx].presil_params.fd_for_umd_daemon = fd_for_dmabuf;
				CAM_INFO(CAM_PRESIL,
					"Received generated idx %d fd_for_dmabuf Buf* %lld", idx,
					fd_for_dmabuf);
			} else {
				fd_for_dmabuf = tbl.bufq[idx].presil_params.fd_for_umd_daemon;
				CAM_INFO(CAM_PRESIL,
					"Received existing at idx %d fd_for_dmabuf Buf* %lld", idx,
					fd_for_dmabuf);
			}

			tbl.bufq[idx].presil_params.refcount++;
		} else {
			CAM_DBG(CAM_MEM,
				"Not found dmabuf at idx=%d, dma_buf %pK handle 0x%0x active %d ",
				idx, tbl.bufq[idx].dma_buf, tbl.bufq[idx].buf_handle,
				tbl.bufq[idx].active);
		}
	}

	return (int)fd_for_dmabuf;
}
EXPORT_SYMBOL(cam_presil_get_fd_from_dmabuf);

int cam_mem_mgr_send_buffer_to_presil(int32_t iommu_hdl, int32_t buf_handle)
{
	int rc = 0;

	/* Sending Presil IO Buf to PC side ( as iova start address indicates) */
	uint64_t io_buf_addr;
	size_t io_buf_size;
	int i, j, fd = -1, idx = 0;
	uint8_t *iova_ptr = NULL;
	uint64_t dmabuf = 0;
	bool is_mapped_in_cb = false;

	CAM_DBG(CAM_PRESIL, "buf handle 0x%0x", buf_handle);

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	for (i = 0; i < tbl.bufq[idx].num_hdl; i++) {
		if (tbl.bufq[idx].hdls[i] == iommu_hdl)
			is_mapped_in_cb = true;
	}

	if (!is_mapped_in_cb) {
		for (j = 0; j < CAM_MEM_BUFQ_MAX; j++) {
			if (tbl.bufq[j].i_ino == tbl.bufq[idx].i_ino) {
				for (i = 0; i < tbl.bufq[j].num_hdl; i++) {
					if (tbl.bufq[j].hdls[i] == iommu_hdl)
						is_mapped_in_cb = true;
				}
			}
		}

		if (!is_mapped_in_cb) {
			CAM_DBG(CAM_PRESIL,
				"Still Could not find idx=%d, FD %d buf_handle 0x%0x",
				idx, GET_FD_FROM_HANDLE(buf_handle), buf_handle);

			/*
			 * Okay to return 0, since this function also gets called for buffers that
			 * are shared only between umd/kmd, these may not be mapped with smmu
			 */
			return 0;
		}
	}

	if ((tbl.bufq[idx].buf_handle != 0) && (tbl.bufq[idx].active) &&
		(tbl.bufq[idx].buf_handle == buf_handle)) {
		CAM_DBG(CAM_PRESIL,
			"Found dmabuf in bufq idx %d, FD %d handle 0x%0x dmabuf %pK",
			idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle, tbl.bufq[idx].dma_buf);
		dmabuf = (uint64_t)tbl.bufq[idx].dma_buf;
		fd = tbl.bufq[idx].fd;
	} else {
		CAM_ERR(CAM_PRESIL,
			"Could not find dmabuf Invalid Mem idx=%d, FD %d handle 0x%0x active %d",
			idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle, tbl.bufq[idx].active);
		return -EINVAL;
	}

	rc = cam_mem_get_io_buf(buf_handle, iommu_hdl, &io_buf_addr, &io_buf_size, NULL);
	if (rc || NULL == (void *)io_buf_addr) {
		CAM_DBG(CAM_PRESIL, "Invalid ioaddr : 0x%x, fd = %d,  dmabuf = %pK",
			io_buf_addr, fd, dmabuf);
		return -EINVAL;
	}

	iova_ptr = (uint8_t *)io_buf_addr;
	CAM_INFO(CAM_PRESIL, "Sending buffer with ioaddr : 0x%x, fd = %d, dmabuf = %pK",
		io_buf_addr, fd, dmabuf);

	rc = cam_presil_send_buffer(dmabuf, 0, 0, (uint32_t)io_buf_size, (uint64_t)iova_ptr);

	return rc;
}

int cam_mem_mgr_send_all_buffers_to_presil(int32_t iommu_hdl)
{
	int idx = 0;
	int rc = 0;
	int32_t fd_already_sent[128];
	int fd_already_sent_count = 0;
	int fd_already_index = 0;
	int fd_already_sent_found = 0;


	memset(&fd_already_sent, 0x0, sizeof(fd_already_sent));

	for (idx = 0; idx < CAM_MEM_BUFQ_MAX; idx++) {
		if ((tbl.bufq[idx].buf_handle != 0) && (tbl.bufq[idx].active)) {
			CAM_DBG(CAM_PRESIL, "Sending %d, FD %d handle 0x%0x", idx, tbl.bufq[idx].fd,
				tbl.bufq[idx].buf_handle);
			fd_already_sent_found = 0;

			for (fd_already_index = 0; fd_already_index < fd_already_sent_count;
				fd_already_index++) {

				if (fd_already_sent[fd_already_index] == tbl.bufq[idx].fd) {
					fd_already_sent_found = 1;
					CAM_DBG(CAM_PRESIL,
						"fd_already_sent %d, FD %d handle 0x%0x flags=0x%0x",
						idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle,
						tbl.bufq[idx].flags);
				}
			}

			if (fd_already_sent_found)
				continue;

			CAM_DBG(CAM_PRESIL, "Sending %d, FD %d handle 0x%0x flags=0x%0x", idx,
				tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle, tbl.bufq[idx].flags);

			rc = cam_mem_mgr_send_buffer_to_presil(iommu_hdl, tbl.bufq[idx].buf_handle);
			fd_already_sent[fd_already_sent_count++] = tbl.bufq[idx].fd;

		} else {
			CAM_DBG(CAM_PRESIL, "Invalid Mem idx=%d, FD %d handle 0x%0x active %d",
				idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle,
				tbl.bufq[idx].active);
		}
	}

	return rc;
}
EXPORT_SYMBOL(cam_mem_mgr_send_all_buffers_to_presil);

int cam_mem_mgr_retrieve_buffer_from_presil(int32_t buf_handle, uint32_t buf_size,
	uint32_t offset, int32_t iommu_hdl)
{
	int rc = 0;

	/* Receive output buffer from Presil IO Buf to PC side (as iova start address indicates) */
	uint64_t io_buf_addr;
	size_t io_buf_size;
	uint64_t dmabuf = 0;
	int fd = 0;
	uint8_t *iova_ptr = NULL;
	int idx = 0;


	CAM_DBG(CAM_PRESIL, "buf handle 0x%0x ", buf_handle);
	rc = cam_mem_get_io_buf(buf_handle, iommu_hdl, &io_buf_addr, &io_buf_size, NULL);
	if (rc) {
		CAM_ERR(CAM_PRESIL, "Unable to get IOVA for buffer buf_hdl: 0x%0x iommu_hdl: 0x%0x",
			buf_handle, iommu_hdl);
		return -EINVAL;
	}

	iova_ptr = (uint8_t *)io_buf_addr;
	iova_ptr += offset;   // correct target address to start writing buffer to.

	if (!buf_size) {
		buf_size = io_buf_size;
		CAM_DBG(CAM_PRESIL, "Updated buf_size from Zero to 0x%0x", buf_size);
	}

	fd = GET_FD_FROM_HANDLE(buf_handle);

	idx = CAM_MEM_MGR_GET_HDL_IDX(buf_handle);
	if ((tbl.bufq[idx].buf_handle != 0) && (tbl.bufq[idx].active) &&
		(tbl.bufq[idx].buf_handle == buf_handle)) {
		CAM_DBG(CAM_PRESIL, "Found dmabuf in bufq idx %d, FD %d handle 0x%0x dmabuf %pK",
			idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle, tbl.bufq[idx].dma_buf);
		dmabuf = (uint64_t)tbl.bufq[idx].dma_buf;
	} else {
		CAM_ERR(CAM_PRESIL,
			"Could not find dmabuf Invalid Mem idx=%d, FD %d handle 0x%0x active %d ",
			idx, tbl.bufq[idx].fd, tbl.bufq[idx].buf_handle, tbl.bufq[idx].active);
	}

	CAM_DBG(CAM_PRESIL,
		"Retrieving buffer with ioaddr : 0x%x, offset = %d, size = %d, fd = %d, dmabuf = %pK",
		io_buf_addr, offset, buf_size, fd, dmabuf);

	rc = cam_presil_retrieve_buffer(dmabuf, 0, 0, (uint32_t)buf_size, (uint64_t)io_buf_addr);

	CAM_INFO(CAM_PRESIL,
		"Retrieved buffer with ioaddr : 0x%x, offset = %d, size = %d, fd = %d, dmabuf = %pK",
		io_buf_addr, 0, buf_size, fd, dmabuf);

	return rc;
}

#else /* ifdef CONFIG_CAM_PRESIL */
struct dma_buf * cam_mem_mgr_get_dma_buf(int fd)
{
	return NULL;
}

int cam_mem_mgr_send_all_buffers_to_presil(int32_t iommu_hdl)
{
       return 0;
}

int cam_mem_mgr_send_buffer_to_presil(int32_t iommu_hdl, int32_t buf_handle)
{
	return 0;
}

int cam_mem_mgr_retrieve_buffer_from_presil(int32_t buf_handle,
	uint32_t buf_size,
	uint32_t offset,
	int32_t iommu_hdl)
{
	return 0;
}
#endif /* ifdef CONFIG_CAM_PRESIL */
