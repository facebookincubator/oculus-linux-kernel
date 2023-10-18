// SPDX-License-Identifier: GPL-2.0-only
/*
 * QTI TEE shared memory bridge driver
 *
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of_platform.h>
#include <linux/of_reserved_mem.h>
#include <linux/qcom_scm.h>
#include <linux/dma-mapping.h>
#include <linux/msm_ion.h>
#include <soc/qcom/qseecomi.h>
#include <linux/qtee_shmbridge.h>
#include <linux/of_platform.h>

#include "qtee_shmbridge_internal.h"

#define DEFAULT_BRIDGE_SIZE	SZ_4M	/*4M*/
#define MIN_BRIDGE_SIZE		SZ_4K   /*4K*/

#define MAXSHMVMS 4
#define PERM_BITS 3
#define VM_BITS 16
#define SELF_OWNER_BIT 1
#define SHM_NUM_VM_SHIFT 9
#define SHM_VM_MASK 0xFFFF
#define SHM_PERM_MASK 0x7

#define VM_PERM_R PERM_READ
#define VM_PERM_W PERM_WRITE

#define SHMBRIDGE_E_NOT_SUPPORTED 4	/* SHMbridge is not implemented */

#define AC_ERR_SHARED_MEMORY_SINGLE_SOURCE 15

/* ns_vmids */
#define UPDATE_NS_VMIDS(ns_vmids, id)	\
				(((uint64_t)(ns_vmids) << VM_BITS) \
				| ((uint64_t)(id) & SHM_VM_MASK))

/* ns_perms */
#define UPDATE_NS_PERMS(ns_perms, perm)	\
				(((uint64_t)(ns_perms) << PERM_BITS) \
				| ((uint64_t)(perm) & SHM_PERM_MASK))

/* pfn_and_ns_perm_flags = paddr | ns_perms */
#define UPDATE_PFN_AND_NS_PERM_FLAGS(paddr, ns_perms)	\
				((uint64_t)(paddr) | (ns_perms))


/* ipfn_and_s_perm_flags = ipaddr | tz_perm */
#define UPDATE_IPFN_AND_S_PERM_FLAGS(ipaddr, tz_perm)	\
				((uint64_t)(ipaddr) | (uint64_t)(tz_perm))

/* size_and_flags when dest_vm is not HYP */
#define UPDATE_SIZE_AND_FLAGS(size, destnum)	\
				((size) | (destnum) << SHM_NUM_VM_SHIFT)

struct bridge_info {
	phys_addr_t paddr;
	void *vaddr;
	size_t size;
	uint64_t handle;
	int min_alloc_order;
	struct gen_pool *genpool;
	struct device *dev;
};

struct bridge_list {
	struct list_head head;
	struct mutex lock;
};

struct bridge_list_entry {
	struct list_head list;
	phys_addr_t paddr;
	uint64_t handle;
};

struct cma_heap_bridge_info {
	uint32_t heapid;
	uint64_t handle;
};

enum CMA_HEAP_TYPE {
	QSEECOM_HEAP = 0,
	QSEECOM_TA_HEAP,
	USER_CONTI_HEAP,
	HEAP_TYPE_MAX
};

static struct bridge_info default_bridge;
static struct bridge_list bridge_list_head;
static bool qtee_shmbridge_enabled;
static bool support_hyp;

/* enable shared memory bridge mechanism in HYP */
static int32_t qtee_shmbridge_enable(bool enable)
{
	int32_t ret = 0;

	qtee_shmbridge_enabled = false;
	if (!enable) {
		pr_warn("shmbridge isn't enabled\n");
		return ret;
	}

	ret = qcom_scm_enable_shm_bridge();

	if (ret) {
		pr_err("Failed to enable shmbridge, ret = %d\n", ret);

		if (ret == -EIO || ret == SHMBRIDGE_E_NOT_SUPPORTED)
			pr_warn("shmbridge is not supported by this target\n");
		return ret;
	}
	qtee_shmbridge_enabled = true;
	pr_warn("shmbridge is enabled\n");
	return ret;
}

/* Check whether shmbridge mechanism is enabled in HYP or not */
bool qtee_shmbridge_is_enabled(void)
{
	return qtee_shmbridge_enabled;
}
EXPORT_SYMBOL(qtee_shmbridge_is_enabled);

static int32_t qtee_shmbridge_list_add_nolock(phys_addr_t paddr,
						uint64_t handle)
{
	struct bridge_list_entry *entry;

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return -ENOMEM;
	entry->handle = handle;
	entry->paddr = paddr;
	list_add_tail(&entry->list, &bridge_list_head.head);
	return 0;
}

static void qtee_shmbridge_list_del_nolock(uint64_t handle)
{
	struct bridge_list_entry *entry;

	list_for_each_entry(entry, &bridge_list_head.head, list) {
		if (entry->handle == handle) {
			list_del(&entry->list);
			kfree(entry);
			break;
		}
	}
}

static int32_t qtee_shmbridge_query_nolock(phys_addr_t paddr)
{
	struct bridge_list_entry *entry;

	list_for_each_entry(entry, &bridge_list_head.head, list)
		if (entry->paddr == paddr) {
			pr_debug("A bridge on %llx exists\n", (uint64_t)paddr);
			return -EEXIST;
		}
	return 0;
}

/* Check whether a bridge starting from paddr exists */
int32_t qtee_shmbridge_query(phys_addr_t paddr)
{
	int32_t ret = 0;

	mutex_lock(&bridge_list_head.lock);
	ret = qtee_shmbridge_query_nolock(paddr);
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_query);

/* Register paddr & size as a bridge, return bridge handle */
int32_t qtee_shmbridge_register(
		phys_addr_t paddr,
		size_t size,
		uint32_t *ns_vmid_list,
		uint32_t *ns_vm_perm_list,
		uint32_t ns_vmid_num,
		uint32_t tz_perm,
		uint64_t *handle)

{
	int32_t ret = 0;
	uint64_t pfn_and_ns_perm_flags = 0;
	uint64_t ipfn_and_s_perm_flags = 0;
	uint64_t size_and_flags = 0;
	uint64_t ns_perms = 0;
	uint64_t ns_vmids = 0;
	int i = 0;

	if (!qtee_shmbridge_enabled)
		return 0;

	if (!handle || !ns_vmid_list || !ns_vm_perm_list ||
				ns_vmid_num > MAXSHMVMS) {
		pr_err("invalid input parameters\n");
		return -EINVAL;
	}

	mutex_lock(&bridge_list_head.lock);
	ret = qtee_shmbridge_query_nolock(paddr);
	if (ret)
		goto exit;

	for (i = 0; i < ns_vmid_num; i++) {
		ns_perms = UPDATE_NS_PERMS(ns_perms, ns_vm_perm_list[i]);
		ns_vmids = UPDATE_NS_VMIDS(ns_vmids, ns_vmid_list[i]);
	}

	pfn_and_ns_perm_flags = UPDATE_PFN_AND_NS_PERM_FLAGS(paddr, ns_perms);
	ipfn_and_s_perm_flags = UPDATE_IPFN_AND_S_PERM_FLAGS(paddr, tz_perm);
	size_and_flags = UPDATE_SIZE_AND_FLAGS(size, ns_vmid_num);

	if (support_hyp) {
		size_and_flags |= SELF_OWNER_BIT << 1;
		size_and_flags |= (VM_PERM_R | VM_PERM_W) << 2;
	}

	pr_debug("%s: desc.args[0] %llx, args[1] %llx, args[2] %llx, args[3] %llx\n",
		__func__, pfn_and_ns_perm_flags, ipfn_and_s_perm_flags,
		size_and_flags, ns_vmids);

	ret = qcom_scm_create_shm_bridge(pfn_and_ns_perm_flags,
			ipfn_and_s_perm_flags, size_and_flags, ns_vmids,
			handle);

	if (ret) {
		pr_err("create shmbridge failed, ret = %d\n", ret);
		if (ret == AC_ERR_SHARED_MEMORY_SINGLE_SOURCE)
			ret = -EEXIST;
		else
			ret = -EINVAL;
		goto exit;
	}

	ret = qtee_shmbridge_list_add_nolock(paddr, *handle);
exit:
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_register);

/* Deregister bridge */
int32_t qtee_shmbridge_deregister(uint64_t handle)
{
	int32_t ret = 0;

	if (!qtee_shmbridge_enabled)
		return 0;

	mutex_lock(&bridge_list_head.lock);

	ret = qcom_scm_delete_shm_bridge(handle);

	if (ret) {
		pr_err("Failed to del bridge %lld, ret = %d\n", handle, ret);
		goto exit;
	}
	qtee_shmbridge_list_del_nolock(handle);

exit:
	mutex_unlock(&bridge_list_head.lock);
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_deregister);


/* Sub-allocate from default kernel bridge created by shmb driver */
int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm)
{
	int32_t ret = 0;
	unsigned long va;

	if (IS_ERR_OR_NULL(shm)) {
		pr_err("qtee_shm is NULL\n");
		ret = -EINVAL;
		goto exit;
	}

	if (size > default_bridge.size) {
		pr_err("requestd size %zu is larger than bridge size %d\n",
			size, default_bridge.size);
		ret = -EINVAL;
		goto exit;
	}

	size = roundup(size, 1 << default_bridge.min_alloc_order);

	va = gen_pool_alloc(default_bridge.genpool, size);
	if (!va) {
		pr_err("failed to sub-allocate %zu bytes from bridge\n", size);
		ret = -ENOMEM;
		goto exit;
	}

	memset((void *)va, 0, size);
	shm->vaddr = (void *)va;
	shm->paddr = gen_pool_virt_to_phys(default_bridge.genpool, va);
	shm->size = size;

	pr_debug("%s: shm->paddr %llx, size %zu\n",
			__func__, (uint64_t)shm->paddr, shm->size);

exit:
	return ret;
}
EXPORT_SYMBOL(qtee_shmbridge_allocate_shm);


/* Free buffer that is sub-allocated from default kernel bridge */
void qtee_shmbridge_free_shm(struct qtee_shm *shm)
{
	if (IS_ERR_OR_NULL(shm) || !shm->vaddr)
		return;
	gen_pool_free(default_bridge.genpool, (unsigned long)shm->vaddr,
		      shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_free_shm);

/* cache clean operation for buffer sub-allocated from default bridge */
void qtee_shmbridge_flush_shm_buf(struct qtee_shm *shm)
{
	if (shm)
		return dma_sync_single_for_device(default_bridge.dev,
				shm->paddr, shm->size, DMA_TO_DEVICE);
}
EXPORT_SYMBOL(qtee_shmbridge_flush_shm_buf);

/* cache invalidation operation for buffer sub-allocated from default bridge */
void qtee_shmbridge_inv_shm_buf(struct qtee_shm *shm)
{
	if (shm)
		return dma_sync_single_for_cpu(default_bridge.dev,
				shm->paddr, shm->size, DMA_FROM_DEVICE);
}
EXPORT_SYMBOL(qtee_shmbridge_inv_shm_buf);

/*
 * shared memory bridge initialization
 *
 */
static int qtee_shmbridge_init(struct platform_device *pdev)
{
	int ret = 0;
	uint32_t custom_bridge_size;
	uint32_t *ns_vm_ids;
	uint32_t ns_vm_ids_hlos[] = {VMID_HLOS};
	uint32_t ns_vm_ids_hyp[] = {};
	uint32_t ns_vm_perms[] = {VM_PERM_R|VM_PERM_W};
	int mem_protection_enabled = 0;

	support_hyp = of_property_read_bool((&pdev->dev)->of_node,
			"qcom,support-hypervisor");
	if (support_hyp)
		ns_vm_ids = ns_vm_ids_hyp;
	else
		ns_vm_ids = ns_vm_ids_hlos;

	if (default_bridge.vaddr) {
		pr_err("qtee shmbridge is already initialized\n");
		return 0;
	}

	ret = of_property_read_u32((&pdev->dev)->of_node,
		"qcom,custom-bridge-size", &custom_bridge_size);
	if (ret)
		default_bridge.size = DEFAULT_BRIDGE_SIZE;
	else
		default_bridge.size = custom_bridge_size * MIN_BRIDGE_SIZE;

	pr_err("qtee shmbridge registered default bridge with size %d bytes\n",
		default_bridge.size);

	default_bridge.vaddr = (void *)__get_free_pages(GFP_KERNEL|__GFP_COMP,
				get_order(default_bridge.size));
	if (!default_bridge.vaddr)
		return -ENOMEM;

	default_bridge.paddr = dma_map_single(&pdev->dev,
				default_bridge.vaddr, default_bridge.size,
				DMA_TO_DEVICE);
	if (dma_mapping_error(&pdev->dev, default_bridge.paddr)) {
		pr_err("dma_map_single() failed\n");
		ret = -ENOMEM;
		goto exit_freebuf;
	}
	default_bridge.dev = &pdev->dev;

	/* create a general mem pool */
	default_bridge.min_alloc_order = PAGE_SHIFT; /* 4K page size aligned */
	default_bridge.genpool = gen_pool_create(
					default_bridge.min_alloc_order, -1);
	if (!default_bridge.genpool) {
		pr_err("gen_pool_add_virt() failed\n");
		ret = -ENOMEM;
		goto exit_unmap;
	}

	gen_pool_set_algo(default_bridge.genpool, gen_pool_best_fit, NULL);
	ret = gen_pool_add_virt(default_bridge.genpool,
			(uintptr_t)default_bridge.vaddr,
				default_bridge.paddr, default_bridge.size, -1);
	if (ret) {
		pr_err("gen_pool_add_virt() failed, ret = %d\n", ret);
		goto exit_destroy_pool;
	}

	mutex_init(&bridge_list_head.lock);
	INIT_LIST_HEAD(&bridge_list_head.head);

	/* temporarily disable shm bridge mechanism */
	ret = qtee_shmbridge_enable(true);
	if (ret) {
		/* keep the mem pool and return if failed to enable bridge */
		ret = 0;
		goto exit;
	}

	/*register default bridge*/
	if (support_hyp)
		ret = qtee_shmbridge_register(default_bridge.paddr,
			default_bridge.size, ns_vm_ids,
			ns_vm_perms, 0, VM_PERM_R|VM_PERM_W,
			&default_bridge.handle);
	else
		ret = qtee_shmbridge_register(default_bridge.paddr,
			default_bridge.size, ns_vm_ids,
			ns_vm_perms, 1, VM_PERM_R|VM_PERM_W,
			&default_bridge.handle);

	if (ret) {
		pr_err("Failed to register default bridge, size %zu\n",
			default_bridge.size);
		goto exit_deregister_default_bridge;
	}

	pr_debug("qtee shmbridge registered default bridge with size %d bytes\n",
			default_bridge.size);

	mem_protection_enabled = scm_mem_protection_init_do();
	pr_debug("MEM protection %s, %d\n",
			(!mem_protection_enabled ? "Enabled" : "Not enabled"),
			mem_protection_enabled);
	return 0;

exit_deregister_default_bridge:
	qtee_shmbridge_deregister(default_bridge.handle);
	qtee_shmbridge_enable(false);
exit_destroy_pool:
	gen_pool_destroy(default_bridge.genpool);
exit_unmap:
	dma_unmap_single(&pdev->dev, default_bridge.paddr, default_bridge.size,
			DMA_TO_DEVICE);
exit_freebuf:
	free_pages((long)default_bridge.vaddr, get_order(default_bridge.size));
	default_bridge.vaddr = NULL;
exit:
	return ret;
}

static int qtee_shmbridge_probe(struct platform_device *pdev)
{
#ifdef CONFIG_ARM64
	dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
#endif
	return qtee_shmbridge_init(pdev);
}

static int qtee_shmbridge_remove(struct platform_device *pdev)
{
	qtee_shmbridge_deregister(default_bridge.handle);
	gen_pool_destroy(default_bridge.genpool);
	dma_unmap_single(&pdev->dev, default_bridge.paddr, default_bridge.size,
			DMA_TO_DEVICE);
	free_pages((long)default_bridge.vaddr, get_order(default_bridge.size));
	return 0;
}

static const struct of_device_id qtee_shmbridge_of_match[] = {
	{ .compatible = "qcom,tee-shared-memory-bridge"},
	{}
};
MODULE_DEVICE_TABLE(of, qtee_shmbridge_of_match);

static struct platform_driver qtee_shmbridge_driver = {
	.probe = qtee_shmbridge_probe,
	.remove = qtee_shmbridge_remove,
	.driver = {
		.name = "shared_memory_bridge",
		.of_match_table = qtee_shmbridge_of_match,
	},
};

int __init qtee_shmbridge_driver_init(void)
{
	return platform_driver_register(&qtee_shmbridge_driver);
}

void __exit qtee_shmbridge_driver_exit(void)
{
	platform_driver_unregister(&qtee_shmbridge_driver);
}
