/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_H
#define __KGSL_H

#include <linux/cdev.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <uapi/linux/msm_kgsl.h>

#include "kgsl_gmu_core.h"
#include "kgsl_pwrscale.h"

#ifdef CONFIG_ARM_LPAE
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(64)
#else
#define KGSL_DMA_BIT_MASK	DMA_BIT_MASK(32)
#endif

/*
 * --- kgsl drawobj flags ---
 * These flags are same as --- drawobj flags ---
 * but renamed to reflect that cmdbatch is renamed to drawobj.
 */
#define KGSL_DRAWOBJ_MEMLIST           KGSL_CMDBATCH_MEMLIST
#define KGSL_DRAWOBJ_MARKER            KGSL_CMDBATCH_MARKER
#define KGSL_DRAWOBJ_SUBMIT_IB_LIST    KGSL_CMDBATCH_SUBMIT_IB_LIST
#define KGSL_DRAWOBJ_CTX_SWITCH        KGSL_CMDBATCH_CTX_SWITCH
#define KGSL_DRAWOBJ_PROFILING         KGSL_CMDBATCH_PROFILING
#define KGSL_DRAWOBJ_PROFILING_KTIME   KGSL_CMDBATCH_PROFILING_KTIME
#define KGSL_DRAWOBJ_END_OF_FRAME      KGSL_CMDBATCH_END_OF_FRAME
#define KGSL_DRAWOBJ_SYNC              KGSL_CMDBATCH_SYNC
#define KGSL_DRAWOBJ_PWR_CONSTRAINT    KGSL_CMDBATCH_PWR_CONSTRAINT

#define kgsl_drawobj_profiling_buffer kgsl_cmdbatch_profiling_buffer


/* The number of memstore arrays limits the number of contexts allowed.
 * If more contexts are needed, update multiple for MEMSTORE_SIZE
 */
#define KGSL_MEMSTORE_SIZE	((int)(PAGE_SIZE * 8))
#define KGSL_MEMSTORE_GLOBAL	(0)
#define KGSL_PRIORITY_MAX_RB_LEVELS 4
#define KGSL_MEMSTORE_MAX	(KGSL_MEMSTORE_SIZE / \
	sizeof(struct kgsl_devmemstore) - 1 - KGSL_PRIORITY_MAX_RB_LEVELS)
#define KGSL_MAX_CONTEXTS_PER_PROC 200

#define KGSL_ID_MEMSTORE(dev, ctxt_id) \
	((struct kgsl_devmemstore *)((dev)->memstore_kptr + \
	 (ctxt_id) * sizeof(struct kgsl_devmemstore)))

#define KGSL_RB_MEMSTORE(dev, rb) \
	((struct kgsl_devmemstore *)((dev)->memstore_kptr + \
	 ((rb)->id + KGSL_MEMSTORE_MAX) * sizeof(struct kgsl_devmemstore)))

#define MEMSTORE_ID_GPU_ADDR(dev, iter, field) \
	((dev)->memstore->gpuaddr + KGSL_MEMSTORE_OFFSET(iter, field))

#define MEMSTORE_RB_GPU_ADDR(dev, rb, field)	\
	((dev)->memstore->gpuaddr + \
	 KGSL_MEMSTORE_OFFSET(((rb)->id + KGSL_MEMSTORE_MAX), field))

/*
 * SCRATCH MEMORY: The scratch memory is one page worth of data that
 * is mapped into the GPU. This allows for some 'shared' data between
 * the GPU and CPU.
 *
 * Used Data:
 * Offset: Length(bytes): What
 * 0x0: 4 * KGSL_PRIORITY_MAX_RB_LEVELS: RB0 RPTR
 */

/* Shadow global helpers */
#define SCRATCH_RPTR_OFFSET(id) ((id) * sizeof(unsigned int))
#define SCRATCH_RPTR_GPU_ADDR(dev, id) \
	((dev)->scratch->gpuaddr + SCRATCH_RPTR_OFFSET(id))

/* OFFSET to KMD postamble packets in scratch buffer */
#define SCRATCH_POSTAMBLE_OFFSET (100 * sizeof(u64))
#define SCRATCH_POSTAMBLE_ADDR(dev) \
	((dev)->scratch->gpuaddr + SCRATCH_POSTAMBLE_OFFSET)

/* Timestamp window used to detect rollovers (half of integer range) */
#define KGSL_TIMESTAMP_WINDOW 0x80000000

/*
 * A macro for memory statistics - add the new size to the stat and if
 * the statisic is greater then _max, set _max
 */
static inline void KGSL_STATS_ADD(uint64_t size, atomic_long_t *stat,
		atomic_long_t *max)
{
	uint64_t ret = atomic_long_add_return(size, stat);

	if (ret > atomic_long_read(max))
		atomic_long_set(max, ret);
}

#define KGSL_MAX_NUMIBS 100000
#define KGSL_MAX_SYNCPOINTS 32

struct kgsl_device;
struct kgsl_context;

/**
 * struct kgsl_driver - main container for global KGSL things
 * @cdev: Character device struct
 * @major: Major ID for the KGSL device
 * @class: Pointer to the class struct for the core KGSL sysfs entries
 * @virtdev: Virtual device for managing the core
 * @ptkobj: kobject for storing the pagetable statistics
 * @prockobj: kobject for storing the process statistics
 * @threadkobj: kobject for storing thread statistics
 * @devp: Array of pointers to the individual KGSL device structs
 * @process_list: List of open processes
 * @pagetable_list: LIst of open pagetables
 * @ptlock: Lock for accessing the pagetable list
 * @process_mutex: Mutex for accessing the process list
 * @proclist_lock: Lock for accessing the process list
 * @devlock: Mutex protecting the device list
 * @stats: Struct containing atomic memory statistics
 * @full_cache_threshold: the threshold that triggers a full cache flush
 * @workqueue: Pointer to a single threaded workqueue
 * @mem_workqueue: Pointer to a workqueue for deferring memory entries
 * @mem_work: Work struct to schedule mem_workqueue flush
 * @destroy_work: Work struct to handle deferred destruction of memory entries
 */
struct kgsl_driver {
	struct cdev cdev;
	dev_t major;
	struct class *class;
	struct device virtdev;
	struct kobject *ptkobj;
	struct kobject *prockobj;
	struct kobject *threadkobj;
	struct kgsl_device *devp[1];
	struct list_head process_list;
	struct list_head thread_list;
	struct list_head pagetable_list;
	spinlock_t ptlock;
	struct mutex process_mutex;
	spinlock_t proclist_lock;
	struct mutex devlock;
	struct {
		atomic_long_t page_alloc;
		atomic_long_t page_alloc_max;
		atomic_long_t secure;
		atomic_long_t secure_max;
		atomic_long_t mapped;
		atomic_long_t mapped_max;
	} stats;
	unsigned int full_cache_threshold;
	struct workqueue_struct *workqueue;
	struct workqueue_struct *mem_workqueue;
	struct work_struct mem_work;
	struct work_struct destroy_work;
	struct kthread_worker worker;
	struct task_struct *worker_thread;
};

extern struct kgsl_driver kgsl_driver;

struct kgsl_pagetable;
struct kgsl_memdesc;

struct kgsl_memdesc_ops {
	unsigned int vmflags;
	int (*vmfault)(struct kgsl_memdesc *memdesc, struct vm_area_struct *vma,
		       struct vm_fault *vmf);
	void (*free)(struct kgsl_memdesc *memdesc, struct list_head *page_list);
};

/* Internal definitions for memdesc->priv */
#define KGSL_MEMDESC_GUARD_PAGE BIT(0)
/* Set if the memdesc is mapped into all pagetables */
#define KGSL_MEMDESC_GLOBAL BIT(1)
/* The memdesc is frozen during a snapshot */
#define KGSL_MEMDESC_FROZEN BIT(2)
/* The memdesc is mapped into a pagetable */
#define KGSL_MEMDESC_MAPPED BIT(3)
/* The memdesc is secured for content protection */
#define KGSL_MEMDESC_SECURE BIT(4)
/* Memory is accessible in privileged mode */
#define KGSL_MEMDESC_PRIVILEGED BIT(6)
/* This is an instruction buffer */
#define KGSL_MEMDESC_UCODE BIT(7)
/* For global buffers, randomly assign an address from the region */
#define KGSL_MEMDESC_RANDOM BIT(8)
/* The kernel has write access to this memdesc's buffer */
#define KGSL_MEMDESC_KERNEL_RW BIT(9)
/* Allocate memory from the system instead of the pools */
#define KGSL_MEMDESC_SYSMEM BIT(10)
/* The memdesc pages can be reclaimed */
#define KGSL_MEMDESC_CAN_RECLAIM BIT(11)
/* The memdesc pages were reclaimed */
#define KGSL_MEMDESC_RECLAIMED BIT(12)
/* Skip reclaim of the memdesc pages */
#define KGSL_MEMDESC_SKIP_RECLAIM BIT(13)
/* Use SHMEM for allocation */
#define KGSL_MEMDESC_USE_SHMEM BIT(14)
/* This entry is backed by fixed physical resources that cannot be freed */
#define KGSL_MEMDESC_FIXED BIT(15)
/* Lazily allocate memory for this entry */
#define KGSL_MEMDESC_LAZY_ALLOCATION BIT(16)

/**
 * struct kgsl_memdesc - GPU memory object descriptor
 * @pagetable: Pointer to the pagetable that the object is mapped in
 * @gpuaddr: GPU virtual address
 * @size: Internal size of the memory object
 * @physsize: Total size of pages backing this object
 * @mapsize: Total size of pages mapped to userspace
 * @priv: Internal flags and settings
 * @sgt: Scatter gather table for allocated pages
 * @ops: Function hooks for the memdesc memory type
 * @flags: Flags set from userspace
 * @shmem_filp: Pointer to the shmem file backing this memdesc
 */
struct kgsl_memdesc {
	struct kgsl_pagetable *pagetable;
	uint64_t gpuaddr;
	uint64_t size;
	atomic_long_t physsize;
	atomic_long_t mapsize;
	unsigned int priv;
	struct sg_table *sgt;
	struct kgsl_memdesc_ops *ops;
	uint64_t flags;
	struct file *shmem_filp;
	/**
	 * @reclaimed_page_count: Total number of pages reclaimed
	 */
	int reclaimed_page_count;
	/*
	 * @gpuaddr_lock: Spinlock to protect the gpuaddr from being accessed by
	 * multiple entities trying to map the same SVM region at once
	 */
	spinlock_t gpuaddr_lock;
};

/**
 * struct kgsl_global_memdesc  - wrapper for global memory objects
 */
struct kgsl_global_memdesc {
	/** @memdesc: Container for the GPU memory descriptor for the object */
	struct kgsl_memdesc memdesc;
	/** @name: Name of the object for the debugfs list */
	const char *name;
	/** @node: List node for the list of global objects */
	struct list_head node;
};

/*
 * List of different memory entry types. The usermem enum
 * starts at 0, which we use for allocated memory, so 1 is
 * added to the enum values.
 */
#define KGSL_MEM_ENTRY_KERNEL 0
#define KGSL_MEM_ENTRY_USER (KGSL_USER_MEM_TYPE_ADDR + 1)
#define KGSL_MEM_ENTRY_ION (KGSL_USER_MEM_TYPE_ION + 1)
#define KGSL_MEM_ENTRY_MAX (KGSL_USER_MEM_TYPE_MAX + 1)

/* symbolic table for trace and debugfs */
#define KGSL_MEM_TYPES \
	{ KGSL_MEM_ENTRY_KERNEL, "gpumem" }, \
	{ KGSL_MEM_ENTRY_USER, "usermem" }, \
	{ KGSL_MEM_ENTRY_ION, "ion" }

/*
 * struct kgsl_mem_entry - a userspace memory allocation
 * @refcount: reference count. Currently userspace can only
 *  hold a single reference count, but the kernel may hold more.
 * @memdesc: description of the memory
 * @destroy_node: Lockless list node for deferred destruction of this entry.
 * @priv_data: type-specific data, such as the dma-buf attachment pointer.
 * @priv: back pointer to the process that owns this memory
 * @metadata: String containing user specified metadata for the entry
 * @id: idr index for this entry, can be used to find memory that does not have
 *  a valid GPU address.
 * @pending_free: if !0, userspace requested that his memory be freed, but there
 *  are still references to it.
 * @dev_priv: back pointer to the device file that created this entry.
 */
struct kgsl_mem_entry {
	struct kref refcount;
	struct kgsl_memdesc memdesc;
	struct llist_node destroy_node;
	void *priv_data;
	struct kgsl_process_private *priv;
#if IS_ENABLED(CONFIG_QCOM_KGSL_ENTRY_METADATA)
	char *metadata;
#endif
	unsigned int id;
	atomic_t pending_free;
	/**
	 * @map_count: Count how many vmas this object is mapped in - used for
	 * debugfs accounting
	 */
	atomic_t map_count;
};

struct kgsl_device_private;
struct kgsl_event_group;

typedef void (*kgsl_event_func)(struct kgsl_device *, struct kgsl_event_group *,
		void *, int);

/**
 * struct kgsl_event - KGSL GPU timestamp event
 * @device: Pointer to the KGSL device that owns the event
 * @context: Pointer to the context that owns the event
 * @timestamp: Timestamp for the event to expire
 * @func: Callback function for for the event when it expires
 * @priv: Private data passed to the callback function
 * @node: List node for the kgsl_event_group list
 * @created: Jiffies when the event was created
 * @work: Work struct for dispatching the callback
 * @result: KGSL event result type to pass to the callback
 * group: The event group this event belongs to
 */
struct kgsl_event {
	struct kgsl_device *device;
	struct kgsl_context *context;
	unsigned int timestamp;
	kgsl_event_func func;
	void *priv;
	struct list_head node;
	unsigned int created;
	struct work_struct work;
	int result;
	struct kgsl_event_group *group;
};

typedef int (*readtimestamp_func)(struct kgsl_device *, void *,
	enum kgsl_timestamp_type, unsigned int *);

/**
 * struct event_group - A list of GPU events
 * @context: Pointer to the active context for the events
 * @lock: Spinlock for protecting the list
 * @events: List of active GPU events
 * @group: Node for the master group list
 * @processed: Last processed timestamp
 * @name: String name for the group (for the debugfs file)
 * @readtimestamp: Function pointer to read a timestamp
 * @priv: Priv member to pass to the readtimestamp function
 */
struct kgsl_event_group {
	struct kgsl_context *context;
	spinlock_t lock;
	struct list_head events;
	struct list_head group;
	unsigned int processed;
	char name[64];
	readtimestamp_func readtimestamp;
	void *priv;
};

long kgsl_ioctl_device_getproperty(struct kgsl_device_private *dev_priv,
					  unsigned int cmd, void *data);
long kgsl_ioctl_device_setproperty(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_device_waittimestamp_ctxtid(struct kgsl_device_private
				*dev_priv, unsigned int cmd, void *data);
long kgsl_ioctl_rb_issueibcmds(struct kgsl_device_private *dev_priv,
				      unsigned int cmd, void *data);
long kgsl_ioctl_submit_commands(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data);
long kgsl_ioctl_cmdstream_readtimestamp_ctxtid(struct kgsl_device_private
					*dev_priv, unsigned int cmd,
					void *data);
long kgsl_ioctl_cmdstream_freememontimestamp_ctxtid(
						struct kgsl_device_private
						*dev_priv, unsigned int cmd,
						void *data);
long kgsl_ioctl_drawctxt_create(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_drawctxt_destroy(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_sharedmem_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_free_id(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_map_user_mem(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_sync_cache(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_sync_cache_bulk(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_sharedmem_flush_cache(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_alloc(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_alloc_id(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpumem_get_info(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_timestamp_event(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_alloc(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_free(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_info(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_import(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_sync(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_gpu_command(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data);
long kgsl_ioctl_gpuobj_set_info(struct kgsl_device_private *dev_priv,
				unsigned int cmd, void *data);
long kgsl_ioctl_gpu_aux_command(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_create(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_wait(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_query(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_fence_get(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_signal(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);
long kgsl_ioctl_timeline_destroy(struct kgsl_device_private *dev_priv,
		unsigned int cmd, void *data);

long kgsl_ioctl_allow_uid_high_priority(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);
long kgsl_ioctl_allow_tid_maximum_priority(struct kgsl_device_private *dev_priv,
					unsigned int cmd, void *data);

void kgsl_mem_entry_destroy(struct kref *kref);
void kgsl_mem_entry_destroy_deferred(struct kref *kref);

struct kgsl_process_private *kgsl_find_dmabuf_allocator(
		struct kgsl_mem_entry *entry);

void kgsl_get_egl_counts(struct kgsl_mem_entry *entry,
			int *egl_surface_count, int *egl_image_count,
			int *total_count);

struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find(struct kgsl_process_private *private, uint64_t gpuaddr);

struct kgsl_mem_entry * __must_check
kgsl_sharedmem_find_id(struct kgsl_process_private *process, unsigned int id);

struct kgsl_mem_entry *gpumem_alloc_entry(struct kgsl_device_private *dev_priv,
				uint64_t size, uint64_t flags, uint32_t priv);
long gpumem_free_entry(struct kgsl_mem_entry *entry);

void kgsl_mmu_add_global(struct kgsl_device *device,
	struct kgsl_memdesc *memdesc, const char *name);
void kgsl_mmu_remove_global(struct kgsl_device *device,
		struct kgsl_memdesc *memdesc);

/* Helper functions */
pgprot_t kgsl_pgprot_modify(struct kgsl_memdesc *memdesc, pgprot_t pgprot);

int kgsl_request_irq(struct platform_device *pdev, const  char *name,
		irq_handler_t handler, void *data);

int __init kgsl_core_init(void);
void kgsl_core_exit(void);

static inline bool kgsl_gpuaddr_in_memdesc(const struct kgsl_memdesc *memdesc,
				uint64_t gpuaddr, uint64_t size)
{
	if (!memdesc)
		return false;

	/* set a minimum size to search for */
	if (!size)
		size = 1;

	/* don't overflow */
	if (size > U64_MAX - gpuaddr)
		return false;

	return (gpuaddr >= memdesc->gpuaddr &&
	    ((gpuaddr + size) <= (memdesc->gpuaddr + memdesc->size)));
}

static inline int timestamp_cmp(unsigned int a, unsigned int b)
{
	/* check for equal */
	if (a == b)
		return 0;

	/* check for greater-than for non-rollover case */
	if ((a > b) && (a - b < KGSL_TIMESTAMP_WINDOW))
		return 1;

	/* check for greater-than for rollover case
	 * note that <= is required to ensure that consistent
	 * results are returned for values whose difference is
	 * equal to the window size
	 */
	a += KGSL_TIMESTAMP_WINDOW;
	b += KGSL_TIMESTAMP_WINDOW;
	return ((a > b) && (a - b <= KGSL_TIMESTAMP_WINDOW)) ? 1 : -1;
}

/**
 * kgsl_schedule_work() - Schedule a work item on the KGSL workqueue
 * @work: work item to schedule
 */
static inline void kgsl_schedule_work(struct work_struct *work)
{
	queue_work(kgsl_driver.workqueue, work);
}

static inline int
kgsl_mem_entry_get(struct kgsl_mem_entry *entry)
{
	if (entry)
		return kref_get_unless_zero(&entry->refcount);
	return 0;
}

static inline void
kgsl_mem_entry_put(struct kgsl_mem_entry *entry)
{
	if (entry)
		kref_put(&entry->refcount, kgsl_mem_entry_destroy);
}

/**
 * kgsl_mem_entry_put_deferred - Puts refcount and triggers deferred
 *  mem_entry destroy when refcount goes to zero.
 * @entry: memory entry to be put.
 *
 * Use this to put a memory entry when we don't want to block
 * the caller while destroying memory entry.
 */
static inline void
kgsl_mem_entry_put_deferred(struct kgsl_mem_entry *entry)
{
	if (entry)
		kref_put(&entry->refcount, kgsl_mem_entry_destroy_deferred);
}

/*
 * kgsl_addr_range_overlap() - Checks if 2 ranges overlap
 * @gpuaddr1: Start of first address range
 * @size1: Size of first address range
 * @gpuaddr2: Start of second address range
 * @size2: Size of second address range
 *
 * Function returns true if the 2 given address ranges overlap
 * else false
 */
static inline bool kgsl_addr_range_overlap(uint64_t gpuaddr1,
		uint64_t size1, uint64_t gpuaddr2, uint64_t size2)
{
	if ((size1 > (U64_MAX - gpuaddr1)) || (size2 > (U64_MAX - gpuaddr2)))
		return false;
	return !(((gpuaddr1 + size1) <= gpuaddr2) ||
		(gpuaddr1 >= (gpuaddr2 + size2)));
}

static inline int kgsl_copy_from_user(void *dest, void __user *src,
		unsigned int ksize, unsigned int usize)
{
	unsigned int copy = ksize < usize ? ksize : usize;

	if (copy == 0)
		return -EINVAL;

	return copy_from_user(dest, src, copy) ? -EFAULT : 0;
}

static inline void __user *to_user_ptr(uint64_t address)
{
	return (void __user *)(uintptr_t)address;
}

static inline void kgsl_gpu_sysfs_add_link(struct kobject *dst,
			struct kobject *src, const char *src_name,
			const char *dst_name)
{
	struct kernfs_node *old;

	if (dst == NULL || src == NULL)
		return;

	old = sysfs_get_dirent(src->sd, src_name);
	if (IS_ERR_OR_NULL(old))
		return;

	kernfs_create_link(dst->sd, dst_name, old);
}

static inline bool kgsl_is_compat_task(void)
{
	return (BITS_PER_LONG == 32) || is_compat_task();
}
#endif /* __KGSL_H */
