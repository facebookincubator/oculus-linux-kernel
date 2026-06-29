/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __KGSL_GMU_CORE_H
#define __KGSL_GMU_CORE_H

#include <linux/rbtree.h>
#include <linux/mailbox_client.h>

/* GMU_DEVICE - Given an KGSL device return the GMU specific struct */
#define GMU_DEVICE_OPS(_a) ((_a)->gmu_core.dev_ops)

#define MAX_GX_LEVELS		16
#define MAX_CX_LEVELS		4
#define MAX_CNOC_LEVELS		2
#define MAX_CNOC_CMDS		6
#define MAX_BW_CMDS		8
#define INVALID_DCVS_IDX	0xFF

#if MAX_CNOC_LEVELS > MAX_GX_LEVELS
#error "CNOC levels cannot exceed GX levels"
#endif

/*
 * These are the different ways the GMU can boot. GMU_WARM_BOOT is waking up
 * from slumber. GMU_COLD_BOOT is booting for the first time. GMU_RESET
 * is a soft reset of the GMU.
 */
enum gmu_core_boot {
	GMU_WARM_BOOT = 0,
	GMU_COLD_BOOT = 1,
	GMU_RESET = 2
};

/* Bits for the flags field in the gmu structure */
enum gmu_core_flags {
	GMU_BOOT_INIT_DONE = 0,
	GMU_HFI_ON,
	GMU_FAULT,
	GMU_DCVS_REPLAY,
	GMU_ENABLED,
	GMU_RSCC_SLEEP_SEQ_DONE,
	GMU_DISABLE_SLUMBER,
};

/*
 * OOB requests values. These range from 0 to 7 and then
 * the BIT() offset into the actual value is calculated
 * later based on the request. This keeps the math clean
 * and easy to ensure not reaching over/under the range
 * of 8 bits.
 */
enum oob_request {
	oob_gpu = 0,
	oob_perfcntr = 1,
	oob_boot_slumber = 6, /* reserved special case */
	oob_dcvs = 7, /* reserved special case */
	oob_max,
};

enum gmu_pwrctrl_mode {
	GMU_FW_START,
	GMU_FW_STOP,
	GMU_SUSPEND,
	GMU_DCVS_NOHFI,
	GMU_NOTIFY_SLUMBER,
	INVALID_POWER_CTRL
};

#define GPU_HW_ACTIVE	0x00
#define GPU_HW_IFPC	0x03
#define GPU_HW_SLUMBER	0x0f

/*
 * Wait time before trying to write the register again.
 * Hopefully the GMU has finished waking up during this delay.
 * This delay must be less than the IFPC main hysteresis or
 * the GMU will start shutting down before we try again.
 */
#define GMU_CORE_WAKEUP_DELAY_US 10

/* Max amount of tries to wake up the GMU. The short retry
 * limit is half of the long retry limit. After the short
 * number of retries, we print an informational message to say
 * exiting IFPC is taking longer than expected. We continue
 * to retry after this until the long retry limit.
 */
#define GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT 100
#define GMU_CORE_LONG_WAKEUP_RETRY_LIMIT 200

#define FENCE_STATUS_WRITEDROPPED0_MASK 0x1
#define FENCE_STATUS_WRITEDROPPED1_MASK 0x2

#define GMU_MAX_PWRLEVELS	2
#define GMU_FREQ_MIN   200000000
#define GMU_FREQ_MAX   500000000

#define GMU_VER_MAJOR(ver) (((ver) >> 28) & 0xF)
#define GMU_VER_MINOR(ver) (((ver) >> 16) & 0xFFF)
#define GMU_VER_STEP(ver) ((ver) & 0xFFFF)
#define GMU_VERSION(major, minor, step) \
	((((major) & 0xF) << 28) | (((minor) & 0xFFF) << 16) | ((step) & 0xFFFF))

#define GMU_INT_WDOG_BITE		BIT(0)
#define GMU_INT_RSCC_COMP		BIT(1)
#define GMU_INT_FENCE_ERR		BIT(3)
#define GMU_INT_DBD_WAKEUP		BIT(4)
#define GMU_INT_HOST_AHB_BUS_ERR	BIT(5)
#define GMU_AO_INT_MASK		\
		(GMU_INT_WDOG_BITE |	\
		GMU_INT_FENCE_ERR |	\
		GMU_INT_HOST_AHB_BUS_ERR)

/* Bitmask for GPU low power mode enabling and hysterisis*/
#define SPTP_ENABLE_MASK (BIT(2) | BIT(0))
#define IFPC_ENABLE_MASK (BIT(1) | BIT(0))

/* Bitmask for RPMH capability enabling */
#define RPMH_INTERFACE_ENABLE	BIT(0)
#define LLC_VOTE_ENABLE			BIT(4)
#define DDR_VOTE_ENABLE			BIT(8)
#define MX_VOTE_ENABLE			BIT(9)
#define CX_VOTE_ENABLE			BIT(10)
#define GFX_VOTE_ENABLE			BIT(11)
#define RPMH_ENABLE_MASK	(RPMH_INTERFACE_ENABLE	| \
				LLC_VOTE_ENABLE		| \
				DDR_VOTE_ENABLE		| \
				MX_VOTE_ENABLE		| \
				CX_VOTE_ENABLE		| \
				GFX_VOTE_ENABLE)

/* Constants for GMU OOBs */
#define OOB_BOOT_OPTION         0
#define OOB_SLUMBER_OPTION      1

/* Gmu FW block header format */
struct gmu_block_header {
	u32 addr;
	u32 size;
	u32 type;
	u32 value;
};

/* GMU Block types */
#define GMU_BLK_TYPE_DATA 0
#define GMU_BLK_TYPE_PREALLOC_REQ 1
#define GMU_BLK_TYPE_CORE_VER 2
#define GMU_BLK_TYPE_CORE_DEV_VER 3
#define GMU_BLK_TYPE_PWR_VER 4
#define GMU_BLK_TYPE_PWR_DEV_VER 5
#define GMU_BLK_TYPE_HFI_VER 6
#define GMU_BLK_TYPE_PREALLOC_PERSIST_REQ 7

/* For GMU Logs*/
#define GMU_LOG_SIZE  SZ_16K

/* For GMU virtual register bank */
#define GMU_VRB_SIZE  SZ_4K

/*
 * GMU Virtual Register Definitions
 * These values are dword offsets into the GMU Virtual Register Bank
 */
enum gmu_vrb_idx {
	/* Number of dwords supported by VRB */
	VRB_SIZE_IDX = 0,
	/* Contains the address of warmboot scratch buffer */
	VRB_WARMBOOT_SCRATCH_IDX = 1,
	/* Contains the address of GMU trace buffer */
	VRB_TRACE_BUFFER_ADDR_IDX = 2,
};

/* For GMU Trace */
#define GMU_TRACE_SIZE  SZ_16K

/* Trace header defines */
/* Logtype to decode the trace pkt data */
#define TRACE_LOGTYPE_HWSCHED	1
/* Trace buffer threshold for GMU to send F2H message */
#define TRACE_BUFFER_THRESHOLD	80
/*
 * GMU Trace timer value to check trace packet consumption. GMU timer handler tracks the
 * readindex, If it's not moved since last timer fired, GMU will send the f2h message to
 * drain trace packets. GMU Trace Timer will be restarted if the readindex is moving.
 */
#define TRACE_TIMEOUT_MSEC	5

/* Trace metadata defines */
/* Trace drop mode hint for GMU to drop trace packets when trace buffer is full */
#define TRACE_MODE_DROP	1
/* Trace buffer header version */
#define TRACE_HEADER_VERSION_1	1

/* Trace packet defines */
#define TRACE_PKT_VALID	1
#define TRACE_PKT_SEQ_MASK	GENMASK(15, 0)
#define TRACE_PKT_SZ_MASK	GENMASK(27, 16)
#define TRACE_PKT_SZ_SHIFT	16
#define TRACE_PKT_VALID_MASK	GENMASK(31, 31)
#define TRACE_PKT_SKIP_MASK	GENMASK(30, 30)
#define TRACE_PKT_VALID_SHIFT	31
#define TRACE_PKT_SKIP_SHIFT	30

#define TRACE_PKT_GET_SEQNUM(hdr) ((hdr) & TRACE_PKT_SEQ_MASK)
#define TRACE_PKT_GET_SIZE(hdr) (((hdr) & TRACE_PKT_SZ_MASK) >> TRACE_PKT_SZ_SHIFT)
#define TRACE_PKT_GET_VALID_FIELD(hdr) (((hdr) & TRACE_PKT_VALID_MASK) >> TRACE_PKT_VALID_SHIFT)
#define TRACE_PKT_GET_SKIP_FIELD(hdr) (((hdr) & TRACE_PKT_SKIP_MASK) >> TRACE_PKT_SKIP_SHIFT)

/*
 * Trace buffer header definition
 * Trace buffer header fields initialized/updated by KGSL and GMU
 * GMU input: Following header fields are initialized by KGSL
 *           - @metadata, @threshold, @size, @cookie, @timeout, @log_type
 *           - @readIndex updated by kgsl when traces messages are consumed.
 * GMU output: Following header fields are initialized by GMU only
 *           - @magic, @payload_offset, @payload_size
 *           - @write_index updated by GMU upon filling the trace messages
 */
struct gmu_trace_header {
	/** @magic: Initialized by GMU to check header is valid or not */
	u32 magic;
	/**
	 * @metadata: Trace buffer metadata.Bit(31) Trace Mode to log tracepoints
	 * messages, Bits [3:0] Version for header format changes.
	 */
	u32 metadata;
	/**
	 * @threshold: % at which GMU to send f2h message to wakeup KMD to consume
	 * tracepoints data. Set it to zero to disable thresholding. Threshold is %
	 * of buffer full condition not the trace packet count. If GMU is continuously
	 * writing to trace buffer makes it buffer full condition when KMD is not
	 * consuming it. So GMU check the how much trace buffer % space is full based
	 * on the threshold % value.If the trace packets are filling over % buffer full
	 * condition GMU will send the f2h message for KMD to drain the trace messages.
	 */
	u32 threshold;
	/** @size: trace buffer allocation size in bytes */
	u32 size;
	/** @read_index: trace buffer read index in dwords */
	u32 read_index;
	/** @write_index: trace buffer write index in dwords */
	u32 write_index;
	/** @payload_offset: trace buffer payload dword offset */
	u32 payload_offset;
	/** @payload_size: trace buffer payload size in dword */
	u32 payload_size;
	/** cookie: cookie data sent through F2H_PROCESS_MESSAGE */
	u64 cookie;
	/**
	 * timeout: GMU Trace Timer value in msec - zero to disable trace timer else
	 * value for GMU trace timerhandler to send HFI msg.
	 */
	u32 timeout;
	/** @log_type: To decode the trace buffer data */
	u32 log_type;
} __packed;

/* Trace ID definition */
enum gmu_trace_id {
	GMU_TRACE_PREEMPT_TRIGGER = 1,
	GMU_TRACE_PREEMPT_DONE = 2,
	GMU_TRACE_MAX,
};

struct trace_preempt_trigger {
	u32 cur_rb;
	u32 next_rb;
	u32 ctx_switch_cntl;
} __packed;

struct trace_preempt_done {
	u32 prev_rb;
	u32 next_rb;
	u32 ctx_switch_cntl;
} __packed;

/**
 * struct kgsl_gmu_trace  - wrapper for gmu trace memory object
 */
struct kgsl_gmu_trace {
	 /** @md: gmu trace memory descriptor */
	struct kgsl_memdesc *md;
	/* @seq_num: GMU trace packet sequence number to detect drop packet count */
	u16 seq_num;
	/* @reset_hdr: To reset trace buffer header incase of invalid packet */
	bool reset_hdr;
};

/* GMU memdesc entries */
#define GMU_KERNEL_ENTRIES		16

enum gmu_mem_type {
	GMU_ITCM = 0,
	GMU_ICACHE,
	GMU_CACHE = GMU_ICACHE,
	GMU_DTCM,
	GMU_DCACHE,
	GMU_NONCACHED_KERNEL, /* GMU VBIF3 uncached VA range: 0x60000000 - 0x7fffffff */
	GMU_NONCACHED_KERNEL_EXTENDED, /* GMU VBIF3 uncached VA range: 0xc0000000 - 0xdfffffff */
	GMU_NONCACHED_USER,
	GMU_MEM_TYPE_MAX,
};

/**
 * struct gmu_memdesc - Gmu shared memory object descriptor
 * @hostptr: Kernel virtual address
 * @gmuaddr: GPU virtual address
 * @physaddr: Physical address of the memory object
 * @size: Size of the memory object
 */
struct gmu_memdesc {
	void *hostptr;
	u32 gmuaddr;
	phys_addr_t physaddr;
	u32 size;
};

struct kgsl_mailbox {
	struct mbox_client client;
	struct mbox_chan *channel;
};

struct icc_path;

struct gmu_vma_node {
	struct rb_node node;
	u32 va;
	u32 size;
};

struct gmu_vma_entry {
	/** @start: Starting virtual address of the vma */
	u32 start;
	/** @size: Size of this vma */
	u32 size;
	/** @next_va: Next available virtual address in this vma */
	u32 next_va;
	/** @lock: Spinlock for synchronization */
	spinlock_t lock;
	/** @vma_root: RB tree root that keeps track of dynamic allocations */
	struct rb_root vma_root;
};

enum {
	GMU_PRIV_FIRST_BOOT_DONE = 0,
	GMU_PRIV_GPU_STARTED,
	GMU_PRIV_HFI_STARTED,
	GMU_PRIV_RSCC_SLEEP_DONE,
	GMU_PRIV_PM_SUSPEND,
	GMU_PRIV_PDC_RSC_LOADED,
};

struct device_node;
struct kgsl_device;
struct kgsl_snapshot;

struct gmu_dev_ops {
	int (*oob_set)(struct kgsl_device *device, enum oob_request req);
	void (*oob_clear)(struct kgsl_device *device, enum oob_request req);
	int (*ifpc_store)(struct kgsl_device *device, unsigned int val);
	unsigned int (*ifpc_show)(struct kgsl_device *device);
	void (*cooperative_reset)(struct kgsl_device *device);
	int (*wait_for_active_transition)(struct kgsl_device *device);
	bool (*scales_bandwidth)(struct kgsl_device *device);
	int (*acd_set)(struct kgsl_device *device, bool val);
	int (*bcl_sid_set)(struct kgsl_device *device, u32 sid_id, u64 sid_val);
	u64 (*bcl_sid_get)(struct kgsl_device *device, u32 sid_id);
	void (*send_nmi)(struct kgsl_device *device, bool force);
	void (*force_first_boot)(struct kgsl_device *device);
};

/**
 * struct gmu_core_device - GMU Core device structure
 * @ptr: Pointer to GMU device structure
 * @dev_ops: Pointer to gmu device operations
 * @flags: GMU flags
 */
struct gmu_core_device {
	void *ptr;
	const struct gmu_dev_ops *dev_ops;
	unsigned long flags;
};

extern struct platform_driver a6xx_gmu_driver;
extern struct platform_driver a6xx_rgmu_driver;
extern struct platform_driver a6xx_hwsched_driver;
extern struct platform_driver gen7_gmu_driver;
extern struct platform_driver gen7_hwsched_driver;

/* GMU core functions */

void __init gmu_core_register(void);
void gmu_core_unregister(void);

bool gmu_core_gpmu_isenabled(struct kgsl_device *device);
bool gmu_core_scales_bandwidth(struct kgsl_device *device);
bool gmu_core_isenabled(struct kgsl_device *device);
int gmu_core_dev_acd_set(struct kgsl_device *device, bool val);
void gmu_core_regread(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int *value);
void gmu_core_regwrite(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int value);

/**
 * gmu_core_blkwrite - Do a bulk I/O write to GMU
 * @device: Pointer to the kgsl device
 * @offsetwords: Destination dword offset
 * @buffer: Pointer to the source buffer
 * @size: Number of bytes to copy
 *
 * Write a series of GMU registers quickly without bothering to spend time
 * logging the register writes. The logging of these writes causes extra
 * delays that could allow IRQs arrive and be serviced before finishing
 * all the writes.
 */
void gmu_core_blkwrite(struct kgsl_device *device, unsigned int offsetwords,
		const void *buffer, size_t size);
void gmu_core_regrmw(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int mask, unsigned int bits);
int gmu_core_dev_oob_set(struct kgsl_device *device, enum oob_request req);
void gmu_core_dev_oob_clear(struct kgsl_device *device, enum oob_request req);
int gmu_core_dev_ifpc_show(struct kgsl_device *device);
int gmu_core_dev_ifpc_store(struct kgsl_device *device, unsigned int val);
int gmu_core_dev_wait_for_active_transition(struct kgsl_device *device);
void gmu_core_dev_cooperative_reset(struct kgsl_device *device);

/**
 * gmu_core_fault_snapshot - Set gmu fault and trigger snapshot
 * @device: Pointer to the kgsl device
 *
 * Set the gmu fault and take snapshot when we hit a gmu fault
 */
void gmu_core_fault_snapshot(struct kgsl_device *device);

/**
 * gmu_core_timed_poll_check() - polling *gmu* register at given offset until
 * its value changed to match expected value. The function times
 * out and returns after given duration if register is not updated
 * as expected.
 *
 * @device: Pointer to KGSL device
 * @offset: Register offset in dwords
 * @expected_ret: expected register value that stops polling
 * @timeout_ms: time in milliseconds to poll the register
 * @mask: bitmask to filter register value to match expected_ret
 */
int gmu_core_timed_poll_check(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout_ms, unsigned int mask);

struct kgsl_memdesc;
struct iommu_domain;

/**
 * gmu_core_map_memdesc - Map the memdesc into the GMU IOMMU domain
 * @domain: Domain to map the memory into
 * @memdesc: Memory descriptor to map
 * @gmuaddr: Virtual GMU address to map the memory into
 * @attrs: Attributes for the mapping
 *
 * Return: 0 on success or -ENOMEM on failure
 */
int gmu_core_map_memdesc(struct iommu_domain *domain, struct kgsl_memdesc *memdesc,
		u64 gmuaddr, int attrs);
void gmu_core_dev_force_first_boot(struct kgsl_device *device);

/**
 * gmu_core_set_vrb_register - set vrb register value at specified index
 * @ptr: vrb host pointer
 * @index: vrb index to write the value
 * @val: value to be writen into vrb
 */
void gmu_core_set_vrb_register(void *ptr, u32 index, u32 val);

/**
 * gmu_core_process_trace_data - Process gmu trace buffer data writes to default linux trace buffer
 * @device: Pointer to KGSL device
 * @dev: GMU device instance
 * @trace: GMU trace memory pointer
 */
void gmu_core_process_trace_data(struct kgsl_device *device,
	struct device *dev, struct kgsl_gmu_trace *trace);

/**
 * gmu_core_is_trace_empty - Check for trace buffer empty/full status
 * @hdr: Pointer to gmu trace header
 *
 * Return: true if readidex equl to writeindex else false
 */
bool gmu_core_is_trace_empty(struct gmu_trace_header *hdr);

/**
 * gmu_core_trace_header_init - Initialize the GMU trace buffer header
 * @trace: Pointer to kgsl gmu trace
 */
void gmu_core_trace_header_init(struct kgsl_gmu_trace *trace);

/**
 * gmu_core_reset_trace_header - Reset GMU trace buffer header
 * @trace: Pointer to kgsl gmu trace
 */
void gmu_core_reset_trace_header(struct kgsl_gmu_trace *trace);

#endif /* __KGSL_GMU_CORE_H */
