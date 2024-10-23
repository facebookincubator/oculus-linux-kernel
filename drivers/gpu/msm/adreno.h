/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2008-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __ADRENO_H
#define __ADRENO_H

#include <linux/iopoll.h>
#include <linux/of.h>
#include "adreno_coresight.h"
#include "adreno_dispatch.h"
#include "adreno_drawctxt.h"
#include "adreno_hwsched.h"
#include "adreno_perfcounter.h"
#include "adreno_profile.h"
#include "adreno_ringbuffer.h"
#include "kgsl_sharedmem.h"

/* Used to point CP to the SMMU record during preemption */
#define SET_PSEUDO_SMMU_INFO 0
/* Used to inform CP where to save preemption data at the time of switch out */
#define SET_PSEUDO_PRIV_NON_SECURE_SAVE_ADDR 1
/* Used to inform CP where to save secure preemption data at the time of switch out */
#define SET_PSEUDO_PRIV_SECURE_SAVE_ADDR 2
/* Used to inform CP where to save per context non-secure data at the time of switch out */
#define SET_PSEUDO_NON_PRIV_SAVE_ADDR 3
/* Used to inform CP where to save preemption counter data at the time of switch out */
#define SET_PSEUDO_COUNTER 4

/* Index to preemption scratch buffer to store current QOS value */
#define QOS_VALUE_IDX KGSL_PRIORITY_MAX_RB_LEVELS

/* ADRENO_DEVICE - Given a kgsl_device return the adreno device struct */
#define ADRENO_DEVICE(device) \
		container_of(device, struct adreno_device, dev)

/* KGSL_DEVICE - given an adreno_device, return the KGSL device struct */
#define KGSL_DEVICE(_dev) (&((_dev)->dev))

/* ADRENO_CONTEXT - Given a context return the adreno context struct */
#define ADRENO_CONTEXT(context) \
		container_of(context, struct adreno_context, base)

/* ADRENO_GPU_DEVICE - Given an adreno device return the GPU specific struct */
#define ADRENO_GPU_DEVICE(_a) ((_a)->gpucore->gpudev)

/*
 * ADRENO_POWER_OPS - Given an adreno device return the GPU specific power
 * ops
 */
#define ADRENO_POWER_OPS(_a) ((_a)->gpucore->gpudev->power_ops)

#define ADRENO_CHIPID_CORE(_id) FIELD_GET(GENMASK(31, 24), _id)
#define ADRENO_CHIPID_MAJOR(_id) FIELD_GET(GENMASK(23, 16), _id)
#define ADRENO_CHIPID_MINOR(_id) FIELD_GET(GENMASK(15, 8), _id)
#define ADRENO_CHIPID_PATCH(_id) FIELD_GET(GENMASK(7, 0), _id)

#define ADRENO_GMU_CHIPID(_id) \
	(FIELD_PREP(GENMASK(31, 24), ADRENO_CHIPID_CORE(_id)) | \
	 FIELD_PREP(GENMASK(23, 16), ADRENO_CHIPID_MAJOR(_id)) | \
	 FIELD_PREP(GENMASK(15, 12), ADRENO_CHIPID_MINOR(_id)) | \
	 FIELD_PREP(GENMASK(11, 8), ADRENO_CHIPID_PATCH(_id)))

#define ADRENO_REV_MAJOR(_rev) FIELD_GET(GENMASK(23, 16), _rev)
#define ADRENO_REV_MINOR(_rev) FIELD_GET(GENMASK(15, 8), _rev)
#define ADRENO_REV_PATCH(_rev) FIELD_GET(GENMASK(7, 0), _rev)

#define ADRENO_GMU_REV(_rev) \
	(FIELD_PREP(GENMASK(31, 24), ADRENO_REV_MAJOR(_rev)) | \
	 FIELD_PREP(GENMASK(23, 16), ADRENO_REV_MINOR(_rev)) | \
	 FIELD_PREP(GENMASK(15, 8), ADRENO_REV_PATCH(_rev)))

/* ADRENO_GPUREV - Return the GPU ID for the given adreno_device */
#define ADRENO_GPUREV(_a) ((_a)->gpucore->gpurev)

/*
 * ADRENO_FEATURE - return true if the specified feature is supported by the GPU
 * core
 */
#define ADRENO_FEATURE(_dev, _bit) \
	((_dev)->gpucore->features & (_bit))

/**
 * ADRENO_QUIRK - return true if the specified quirk is required by the GPU
 */
#define ADRENO_QUIRK(_dev, _bit) \
	((_dev)->quirks & (_bit))

#define ADRENO_FW(a, f)		(&(a->fw[f]))

/* Adreno core features */
/* The core supports SP/TP hw controlled power collapse */
#define ADRENO_SPTP_PC BIT(0)
/* The GPU supports content protection */
#define ADRENO_CONTENT_PROTECTION BIT(1)
/* The GPU supports preemption */
#define ADRENO_PREEMPTION BIT(2)
/* The GPMU supports Limits Management */
#define ADRENO_LM BIT(3)
/* The GPU supports retention for cpz registers */
#define ADRENO_CPZ_RETENTION BIT(4)
/* The core has soft fault detection available */
#define ADRENO_SOFT_FAULT_DETECT BIT(5)
/* The GMU supports IFPC power management*/
#define ADRENO_IFPC BIT(6)
/* The core supports IO-coherent memory */
#define ADRENO_IOCOHERENT BIT(7)
/*
 * The GMU supports Adaptive Clock Distribution (ACD)
 * for droop mitigation
 */
#define ADRENO_ACD BIT(8)
/* Cooperative reset enabled GMU */
#define ADRENO_COOP_RESET BIT(9)
/* Indicates that the specific target is no longer supported */
#define ADRENO_DEPRECATED BIT(10)
/* The target supports ringbuffer level APRIV */
#define ADRENO_APRIV BIT(11)
/* The GMU supports Battery Current Limiting */
#define ADRENO_BCL BIT(12)
/* (DEPRECATED) L3 voting is supported with L3 constraints */
#define ADRENO_L3_VOTE BIT(13)
/* LPAC is supported  */
#define ADRENO_LPAC BIT(14)
/* Late Stage Reprojection (LSR) enablment for GMU */
#define ADRENO_LSR BIT(15)
/* GMU and kernel supports hardware fences */
#define ADRENO_HW_FENCE BIT(16)
/* Dynamic Mode Switching supported on this target */
#define ADRENO_DMS BIT(17)


/*
 * Adreno GPU quirks - control bits for various workarounds
 */

/* Set TWOPASSUSEWFI in PC_DBG_ECO_CNTL (5XX/6XX) */
#define ADRENO_QUIRK_TWO_PASS_USE_WFI BIT(0)
/* Submit critical packets at GPU wake up */
#define ADRENO_QUIRK_CRITICAL_PACKETS BIT(1)
/* Mask out RB1-3 activity signals from HW hang detection logic */
#define ADRENO_QUIRK_FAULT_DETECT_MASK BIT(2)
/* Disable RB sampler datapath clock gating optimization */
#define ADRENO_QUIRK_DISABLE_RB_DP2CLOCKGATING BIT(3)
/* Disable local memory(LM) feature to avoid corner case error */
#define ADRENO_QUIRK_DISABLE_LMLOADKILL BIT(4)
/* Allow HFI to use registers to send message to GMU */
#define ADRENO_QUIRK_HFI_USE_REG BIT(5)
/* Only set protected SECVID registers once */
#define ADRENO_QUIRK_SECVID_SET_ONCE BIT(6)
/*
 * Limit number of read and write transactions from
 * UCHE block to GBIF to avoid possible deadlock
 * between GBIF, SMMU and MEMNOC.
 */
#define ADRENO_QUIRK_LIMIT_UCHE_GBIF_RW BIT(8)
/* Do explicit mode control of cx gdsc */
#define ADRENO_QUIRK_CX_GDSC BIT(9)

/* Command identifiers */
#define CONTEXT_TO_MEM_IDENTIFIER	0x2EADBEEF
#define CMD_IDENTIFIER			0x2EEDFACE
#define CMD_INTERNAL_IDENTIFIER		0x2EEDD00D
#define START_IB_IDENTIFIER		0x2EADEABE
#define END_IB_IDENTIFIER		0x2ABEDEAD
#define START_PROFILE_IDENTIFIER	0x2DEFADE1
#define END_PROFILE_IDENTIFIER		0x2DEFADE2
#define PWRON_FIXUP_IDENTIFIER		0x2AFAFAFA

/* One cannot wait forever for the core to idle, so set an upper limit to the
 * amount of time to wait for the core to go idle
 */
#define ADRENO_IDLE_TIMEOUT (20 * 1000)

#define ADRENO_FW_PFP 0
#define ADRENO_FW_SQE 0
#define ADRENO_FW_PM4 1

enum adreno_gpurev {
	ADRENO_REV_UNKNOWN = 0,
	ADRENO_REV_A304 = 304,
	ADRENO_REV_A305 = 305,
	ADRENO_REV_A305C = 306,
	ADRENO_REV_A306 = 307,
	ADRENO_REV_A306A = 308,
	ADRENO_REV_A310 = 310,
	ADRENO_REV_A320 = 320,
	ADRENO_REV_A330 = 330,
	ADRENO_REV_A305B = 335,
	ADRENO_REV_A405 = 405,
	ADRENO_REV_A418 = 418,
	ADRENO_REV_A420 = 420,
	ADRENO_REV_A430 = 430,
	ADRENO_REV_A505 = 505,
	ADRENO_REV_A506 = 506,
	ADRENO_REV_A508 = 508,
	ADRENO_REV_A510 = 510,
	ADRENO_REV_A512 = 512,
	ADRENO_REV_A530 = 530,
	ADRENO_REV_A540 = 540,
	ADRENO_REV_A610 = 610,
	ADRENO_REV_A612 = 612,
	ADRENO_REV_A615 = 615,
	ADRENO_REV_A616 = 616,
	ADRENO_REV_A618 = 618,
	ADRENO_REV_A619 = 619,
	ADRENO_REV_A620 = 620,
	ADRENO_REV_A621 = 621,
	ADRENO_REV_A630 = 630,
	ADRENO_REV_A635 = 635,
	ADRENO_REV_A640 = 640,
	ADRENO_REV_A650 = 650,
	ADRENO_REV_A660 = 660,
	ADRENO_REV_A662 = 662,
	ADRENO_REV_A680 = 680,
	/*
	 * Version numbers may exceed 1 digit
	 * Bits 16-23: Major
	 * Bits 8-15: Minor
	 * Bits 0-7: Patch id
	 */
	ADRENO_REV_GEN6_3_26_0 = 0x032600,
	ADRENO_REV_GEN7_0_0 = 0x070000,
	ADRENO_REV_GEN7_0_1 = 0x070001,
	ADRENO_REV_GEN7_4_0 = 0x070400,
	ADRENO_REV_GEN7_3_0 = 0x070300,
	ADRENO_REV_GEN7_6_0 = 0x070600,
};

#define ADRENO_SOFT_FAULT BIT(0)
#define ADRENO_HARD_FAULT BIT(1)
#define ADRENO_TIMEOUT_FAULT BIT(2)
#define ADRENO_IOMMU_PAGE_FAULT BIT(3)
#define ADRENO_PREEMPT_FAULT BIT(4)
#define ADRENO_GMU_FAULT BIT(5)
#define ADRENO_CTX_DETATCH_TIMEOUT_FAULT BIT(6)
#define ADRENO_GMU_FAULT_SKIP_SNAPSHOT BIT(7)

/* number of throttle counters for DCVS adjustment */
#define ADRENO_GPMU_THROTTLE_COUNTERS 4

struct adreno_gpudev;

/* Time to allow preemption to complete (in ms) */
#define ADRENO_PREEMPT_TIMEOUT 10000

#define PREEMPT_SCRATCH_OFFSET(id) (id * sizeof(u64))

#define PREEMPT_SCRATCH_ADDR(dev, id) \
	((dev)->preempt.scratch->gpuaddr + PREEMPT_SCRATCH_OFFSET(id))

/**
 * enum adreno_preempt_states
 * ADRENO_PREEMPT_NONE: No preemption is scheduled
 * ADRENO_PREEMPT_START: The S/W has started
 * ADRENO_PREEMPT_TRIGGERED: A preeempt has been triggered in the HW
 * ADRENO_PREEMPT_FAULTED: The preempt timer has fired
 * ADRENO_PREEMPT_PENDING: The H/W has signaled preemption complete
 * ADRENO_PREEMPT_COMPLETE: Preemption could not be finished in the IRQ handler,
 * worker has been scheduled
 */
enum adreno_preempt_states {
	ADRENO_PREEMPT_NONE = 0,
	ADRENO_PREEMPT_START,
	ADRENO_PREEMPT_TRIGGERED,
	ADRENO_PREEMPT_FAULTED,
	ADRENO_PREEMPT_PENDING,
	ADRENO_PREEMPT_COMPLETE,
};

/**
 * struct adreno_protected_regs - container for a protect register span
 */
struct adreno_protected_regs {
	/** @reg: Physical protected mode register to write to */
	u32 reg;
	/** @start: Dword offset of the starting register in the range */
	u32 start;
	/**
	 * @end: Dword offset of the ending register in the range
	 * (inclusive)
	 */
	u32 end;
	/**
	 * @noaccess: 1 if the register should not be accessible from
	 * userspace, 0 if it can be read (but not written)
	 */
	u32 noaccess;
};

/**
 * struct adreno_preemption
 * @state: The current state of preemption
 * @scratch: Per-target scratch memory for implementation specific functionality
 * @timer: A timer to make sure preemption doesn't stall
 * @work: A work struct for the preemption worker (for 5XX)
 * preempt_level: The level of preemption (for 6XX)
 * skipsaverestore: To skip saverestore during L1 preemption (for 6XX)
 * usesgmem: enable GMEM save/restore across preemption (for 6XX)
 * count: Track the number of preemptions triggered
 * @postamble_len: Number of dwords in KMD postamble pm4 packet
 */
struct adreno_preemption {
	atomic_t state;
	struct kgsl_memdesc *scratch;
	struct timer_list timer;
	struct work_struct work;
	unsigned int preempt_level;
	bool skipsaverestore;
	bool usesgmem;
	unsigned int count;
	u32 postamble_len;
};

struct adreno_busy_data {
	unsigned int gpu_busy;
	unsigned int bif_ram_cycles;
	unsigned int bif_ram_cycles_read_ch1;
	unsigned int bif_ram_cycles_write_ch0;
	unsigned int bif_ram_cycles_write_ch1;
	unsigned int bif_starved_ram;
	unsigned int bif_starved_ram_ch1;
	unsigned int num_ifpc;
	unsigned int throttle_cycles[ADRENO_GPMU_THROTTLE_COUNTERS];
	u32 bcl_throttle;
};

/**
 * struct adreno_firmware - Struct holding fw details
 * @fwvirt: Buffer which holds the ucode
 * @size: Size of ucode buffer
 * @version: Version of ucode
 * @memdesc: Memory descriptor which holds ucode buffer info
 */
struct adreno_firmware {
	unsigned int *fwvirt;
	size_t size;
	unsigned int version;
	struct kgsl_memdesc *memdesc;
};

/**
 * struct adreno_perfcounter_list_node - struct to store perfcounters
 * allocated by a process on a kgsl fd.
 * @groupid: groupid of the allocated perfcounter
 * @countable: countable assigned to the allocated perfcounter
 * @node: list node for perfcounter_list of a process
 */
struct adreno_perfcounter_list_node {
	unsigned int groupid;
	unsigned int countable;
	struct list_head node;
};

/**
 * struct adreno_device_private - Adreno private structure per fd
 * @dev_priv: the kgsl device private structure
 * @perfcounter_list: list of perfcounters used by the process
 */
struct adreno_device_private {
	struct kgsl_device_private dev_priv;
	struct list_head perfcounter_list;
};

/**
 * struct adreno_reglist_list - A container for list of registers and
 * number of registers in the list
 */
struct adreno_reglist_list {
	/** @reg: List of register **/
	const u32 *regs;
	/** @count: Number of registers in the list **/
	u32 count;
};

/**
 * struct adreno_power_ops - Container for target specific power up/down
 * sequences
 */
struct adreno_power_ops {
	/**
	 * @first_open: Target specific function triggered when first kgsl
	 * instance is opened
	 */
	int (*first_open)(struct adreno_device *adreno_dev);
	/**
	 * @last_close: Target specific function triggered when last kgsl
	 * instance is closed
	 */
	int (*last_close)(struct adreno_device *adreno_dev);
	/**
	 * @active_count_get: Target specific function to keep gpu from power
	 * collapsing
	 */
	int (*active_count_get)(struct adreno_device *adreno_dev);
	/**
	 * @active_count_put: Target specific function to allow gpu to power
	 * collapse
	 */
	void (*active_count_put)(struct adreno_device *adreno_dev);
	/** @pm_suspend: Target specific function to suspend the driver */
	int (*pm_suspend)(struct adreno_device *adreno_dev);
	/** @pm_resume: Target specific function to resume the driver */
	void (*pm_resume)(struct adreno_device *adreno_dev);
	/**
	 * @touch_wakeup: Target specific function to start gpu on touch event
	 */
	void (*touch_wakeup)(struct adreno_device *adreno_dev);
	/** @gpu_clock_set: Target specific function to set gpu frequency */
	int (*gpu_clock_set)(struct adreno_device *adreno_dev, u32 pwrlevel);
	/** @gpu_bus_set: Target specific function to set gpu bandwidth */
	int (*gpu_bus_set)(struct adreno_device *adreno_dev, int bus_level,
		u32 ab);
	/** @register_gdsc_notifier: Target specific function to register gdsc notifier */
	int (*register_gdsc_notifier)(struct adreno_device *adreno_dev);
};

/**
 * struct adreno_gpu_core - A specific GPU core definition
 * @gpurev: Unique GPU revision identifier
 * @core: Match for the core version of the GPU
 * @major: Match for the major version of the GPU
 * @minor: Match for the minor version of the GPU
 * @patchid: Match for the patch revision of the GPU
 * @features: Common adreno features supported by this core
 * @gpudev: Pointer to the GPU family specific functions for this core
 * @uche_gmem_alignment: Alignment required for UCHE GMEM base
 * @gmem_size: Amount of binning memory (GMEM/OCMEM) to reserve for the core
 * @bus_width: Bytes transferred in 1 cycle
 */
struct adreno_gpu_core {
	enum adreno_gpurev gpurev;
	/** @chipid: Unique GPU chipid for external identification */
	u32 chipid;
	unsigned int core, major, minor, patchid;
	/**
	 * @compatible: If specified, use the compatible string to match the
	 * device
	 */
	const char *compatible;
	unsigned long features;
	const struct adreno_gpudev *gpudev;
	const struct adreno_perfcounters *perfcounters;
	u32 uche_gmem_alignment;
	size_t gmem_size;
	u32 bus_width;
	/** @snapshot_size: Size of the static snapshot region in bytes */
	u32 snapshot_size;
};

/**
 * struct adreno_dispatch_ops - Common functions for dispatcher operations
 */
struct adreno_dispatch_ops {
	/* @close: Shut down the dispatcher */
	void (*close)(struct adreno_device *adreno_dev);
	/* @queue_cmds: Queue a command on the context */
	int (*queue_cmds)(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		u32 count, u32 *timestamp);
	/* @queue_context: Queue a context to be dispatched */
	void (*queue_context)(struct adreno_device *adreno_dev,
			struct adreno_context *drawctxt);
	void (*setup_context)(struct adreno_device *adreno_dev,
			struct adreno_context *drawctxt);
	void (*fault)(struct adreno_device *adreno_dev, u32 fault);
	/* @idle: Wait for dipatcher to become idle */
	int (*idle)(struct adreno_device *adreno_dev);
	/* @create_hw_fence: Create a hardware fence */
	void (*create_hw_fence)(struct adreno_device *adreno_dev, struct kgsl_sync_fence *kfence);
};

/**
 * struct adreno_device - The mothership structure for all adreno related info
 * @dev: Reference to struct kgsl_device
 * @priv: Holds the private flags specific to the adreno_device
 * @chipid: Chip ID specific to the GPU
 * @cx_misc_len: Length of the CX MISC register block
 * @cx_misc_virt: Pointer where the CX MISC block is mapped
 * @isense_base: Base physical address of isense block
 * @isense_len: Length of the isense register block
 * @isense_virt: Pointer where isense block is mapped
 * @gpucore: Pointer to the adreno_gpu_core structure
 * @pfp_fw: Buffer which holds the pfp ucode
 * @pfp_fw_size: Size of pfp ucode buffer
 * @pfp_fw_version: Version of pfp ucode
 * @pfp: Memory descriptor which holds pfp ucode buffer info
 * @pm4_fw: Buffer which holds the pm4 ucode
 * @pm4_fw_size: Size of pm4 ucode buffer
 * @pm4_fw_version: Version of pm4 ucode
 * @pm4: Memory descriptor which holds pm4 ucode buffer info
 * @gpmu_cmds_size: Length of gpmu cmd stream
 * @gpmu_cmds: gpmu cmd stream
 * @ringbuffers: Array of pointers to adreno_ringbuffers
 * @num_ringbuffers: Number of ringbuffers for the GPU
 * @cur_rb: Pointer to the current ringbuffer
 * @next_rb: Ringbuffer we are switching to during preemption
 * @prev_rb: Ringbuffer we are switching from during preemption
 * @fast_hang_detect: Software fault detection availability
 * @ft_policy: Defines the fault tolerance policy
 * @long_ib_detect: Long IB detection availability
 * @cooperative_reset: Indicates if graceful death handshake is enabled
 * between GMU and GPU
 * @profile: Container for adreno profiler information
 * @dispatcher: Container for adreno GPU dispatcher
 * @pwron_fixup: Command buffer to run a post-power collapse shader workaround
 * @pwron_fixup_dwords: Number of dwords in the command buffer
 * @input_work: Work struct for turning on the GPU after a touch event
 * @busy_data: Struct holding GPU VBIF busy stats
 * @ram_cycles_lo: Number of DDR clock cycles for the monitor session (Only
 * DDR channel 0 read cycles in case of GBIF)
 * @ram_cycles_lo_ch1_read: Number of DDR channel 1 Read clock cycles for
 * the monitor session
 * @ram_cycles_lo_ch0_write: Number of DDR channel 0 Write clock cycles for
 * the monitor session
 * @ram_cycles_lo_ch1_write: Number of DDR channel 0 Write clock cycles for
 * the monitor session
 * @starved_ram_lo: Number of cycles VBIF/GBIF is stalled by DDR (Only channel 0
 * stall cycles in case of GBIF)
 * @starved_ram_lo_ch1: Number of cycles GBIF is stalled by DDR channel 1
 * @halt: Atomic variable to check whether the GPU is currently halted
 * @pending_irq_refcnt: Atomic variable to keep track of running IRQ handlers
 * @ctx_d_debugfs: Context debugfs node
 * @profile_buffer: Memdesc holding the drawobj profiling buffer
 * @profile_index: Index to store the start/stop ticks in the profiling
 * buffer
 * @pwrup_reglist: Memdesc holding the power up register list
 * which is used by CP during preemption and IFPC
 * @lm_sequence: Pointer to the start of the register write sequence for LM
 * @lm_size: The dword size of the LM sequence
 * @lm_limit: limiting value for LM
 * @lm_threshold_count: register value for counter for lm threshold breakin
 * @lm_threshold_cross: number of current peaks exceeding threshold
 * @ifpc_count: Number of times the GPU went into IFPC
 * @highest_bank_bit: Value of the highest bank bit
 * @csdev: Pointer to a coresight device (if applicable)
 * @gpmu_throttle_counters - counteers for number of throttled clocks
 * @irq_storm_work: Worker to handle possible interrupt storms
 * @active_list: List to track active contexts
 * @active_list_lock: Lock to protect active_list
 * @gpu_llc_slice: GPU system cache slice descriptor
 * @gpu_llc_slice_enable: To enable the GPU system cache slice or not
 * @gpuhtw_llc_slice: GPU pagetables system cache slice descriptor
 * @gpuhtw_llc_slice_enable: To enable the GPUHTW system cache slice or not
 * @zap_loaded: Used to track if zap was successfully loaded or not
 */
struct adreno_device {
	struct kgsl_device dev;    /* Must be first field in this struct */
	unsigned long priv;
	unsigned int chipid;
	/** @uche_gmem_base: Base address of GMEM for UCHE access */
	u64 uche_gmem_base;
	unsigned long cx_dbgc_base;
	unsigned int cx_dbgc_len;
	void __iomem *cx_dbgc_virt;
	unsigned int cx_misc_len;
	void __iomem *cx_misc_virt;
	unsigned long isense_base;
	unsigned int isense_len;
	void __iomem *isense_virt;
	const struct adreno_gpu_core *gpucore;
	struct adreno_firmware fw[2];
	size_t gpmu_cmds_size;
	unsigned int *gpmu_cmds;
	struct adreno_ringbuffer ringbuffers[KGSL_PRIORITY_MAX_RB_LEVELS];
	int num_ringbuffers;
	struct adreno_ringbuffer *cur_rb;
	struct adreno_ringbuffer *next_rb;
	struct adreno_ringbuffer *prev_rb;
	unsigned int fast_hang_detect;
	unsigned long ft_policy;
	bool long_ib_detect;
	bool cooperative_reset;
	struct adreno_profile profile;
	struct adreno_dispatcher dispatcher;
	struct kgsl_memdesc *pwron_fixup;
	unsigned int pwron_fixup_dwords;
	struct work_struct input_work;
	struct adreno_busy_data busy_data;
	unsigned int ram_cycles_lo;
	unsigned int ram_cycles_lo_ch1_read;
	unsigned int ram_cycles_lo_ch0_write;
	unsigned int ram_cycles_lo_ch1_write;
	unsigned int starved_ram_lo;
	unsigned int starved_ram_lo_ch1;
	atomic_t halt;
	atomic_t pending_irq_refcnt;
	struct dentry *ctx_d_debugfs;
	/** @lm_enabled: True if limits management is enabled for this target */
	bool lm_enabled;
	/** @acd_enabled: True if acd is enabled for this target */
	bool acd_enabled;
	/** @hwcg_enabled: True if hardware clock gating is enabled */
	bool hwcg_enabled;
	/** @throttling_enabled: True if LM throttling is enabled on a5xx */
	bool throttling_enabled;
	/** @sptp_pc_enabled: True if SPTP power collapse is enabled on a5xx */
	bool sptp_pc_enabled;
	/** @bcl_enabled: True if BCL is enabled */
	bool bcl_enabled;
	/** @lpac_enabled: True if LPAC is enabled */
	bool lpac_enabled;
	/** @dms_enabled: True if DMS is enabled */
	bool dms_enabled;
	struct kgsl_memdesc *profile_buffer;
	unsigned int profile_index;
	struct kgsl_memdesc *pwrup_reglist;
	uint32_t *lm_sequence;
	uint32_t lm_size;
	struct adreno_preemption preempt;
	struct work_struct gpmu_work;
	uint32_t lm_leakage;
	uint32_t lm_limit;
	uint32_t lm_threshold_count;
	uint32_t lm_threshold_cross;
	uint32_t ifpc_count;

	unsigned int highest_bank_bit;
	unsigned int quirks;

	struct coresight_device *csdev[2];
	uint32_t gpmu_throttle_counters[ADRENO_GPMU_THROTTLE_COUNTERS];
	struct work_struct irq_storm_work;

	struct list_head active_list;
	spinlock_t active_list_lock;

	void *gpu_llc_slice;
	bool gpu_llc_slice_enable;
	void *gpuhtw_llc_slice;
	bool gpuhtw_llc_slice_enable;
	/** @gpumv_llc_slice: GPU MV buffer system cache slice descriptor*/
	void *gpumv_llc_slice;
	/** @gpumv_llc_slice_enable: To enable GPUMV buffer system cache slice or not */
	bool gpumv_llc_slice_enable;
	unsigned int zap_loaded;
	/**
	 * @critpkts: Memory descriptor for 5xx critical packets if applicable
	 */
	struct kgsl_memdesc *critpkts;
	/**
	 * @critpkts: Memory descriptor for 5xx secure critical packets
	 */
	struct kgsl_memdesc *critpkts_secure;
	/** @irq_mask: The current interrupt mask for the GPU device */
	u32 irq_mask;
	/*
	 * @soft_ft_regs: an array of registers for soft fault detection on a3xx
	 * targets
	 */
	u32 *soft_ft_regs;
	/*
	 * @soft_ft_vals: an array of register values for soft fault detection
	 * on a3xx targets
	 */
	u32 *soft_ft_vals;
	/*
	 * @soft_ft_vals: number of elements in @soft_ft_regs and @soft_ft_vals
	 */
	int soft_ft_count;
	/** @wake_on_touch: If true our last wakeup was due to a touch event */
	bool wake_on_touch;
	/* @dispatch_ops: A pointer to a set of adreno dispatch ops */
	const struct adreno_dispatch_ops *dispatch_ops;
	/** @hwsched: Container for the hardware dispatcher */
	struct adreno_hwsched hwsched;
	/*
	 * @perfcounter: Flag to clear perfcounters across contexts and
	 * controls perfcounter ioctl read
	 */
	bool perfcounter;
	/** @gmu_hub_clk_freq: Gmu hub interface clock frequency */
	u64 gmu_hub_clk_freq;
	/* @patch_reglist: If false power up register list needs to be patched */
	bool patch_reglist;
	/*
	 * @uche_client_pf: uche_client_pf client register configuration
	 * for pf debugging
	 */
	u32 uche_client_pf;
	/**
	 * @bcl_data: bit 0 contains response type for bcl alarms and bits 1:21 controls
	 * throttle level for bcl alarm levels 0-2. If not set, gmu fw sets default throttle levels.
	 */
	u32 bcl_data;
	/*
	 * @bcl_debugfs_dir: Debugfs directory node for bcl related nodes
	 */
	struct dentry *bcl_debugfs_dir;
	/** @bcl_throttle_time_us: Total time in us spent in BCL throttling */
	u32 bcl_throttle_time_us;
};

/**
 * enum adreno_device_flags - Private flags for the adreno_device
 * @ADRENO_DEVICE_PWRON - Set during init after a power collapse
 * @ADRENO_DEVICE_PWRON_FIXUP - Set if the target requires the shader fixup
 * after power collapse
 * @ADRENO_DEVICE_CORESIGHT - Set if the coresight (trace bus) registers should
 * be restored after power collapse
 * @ADRENO_DEVICE_STARTED - Set if the device start sequence is in progress
 * @ADRENO_DEVICE_FAULT - Set if the device is currently in fault (and shouldn't
 * send any more commands to the ringbuffer)
 * @ADRENO_DEVICE_DRAWOBJ_PROFILE - Set if the device supports drawobj
 * profiling via the ALWAYSON counter
 * @ADRENO_DEVICE_PREEMPTION - Turn on/off preemption
 * @ADRENO_DEVICE_SOFT_FAULT_DETECT - Set if soft fault detect is enabled
 * @ADRENO_DEVICE_GPMU_INITIALIZED - Set if GPMU firmware initialization succeed
 * @ADRENO_DEVICE_ISDB_ENABLED - Set if the Integrated Shader DeBugger is
 * attached and enabled
 * @ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED - Set if a CACHE_FLUSH_TS irq storm
 * is in progress
 */
enum adreno_device_flags {
	ADRENO_DEVICE_PWRON = 0,
	ADRENO_DEVICE_PWRON_FIXUP = 1,
	ADRENO_DEVICE_INITIALIZED = 2,
	ADRENO_DEVICE_CORESIGHT = 3,
	ADRENO_DEVICE_STARTED = 5,
	ADRENO_DEVICE_FAULT = 6,
	ADRENO_DEVICE_DRAWOBJ_PROFILE = 7,
	ADRENO_DEVICE_GPU_REGULATOR_ENABLED = 8,
	ADRENO_DEVICE_PREEMPTION = 9,
	ADRENO_DEVICE_SOFT_FAULT_DETECT = 10,
	ADRENO_DEVICE_GPMU_INITIALIZED = 11,
	ADRENO_DEVICE_ISDB_ENABLED = 12,
	ADRENO_DEVICE_CACHE_FLUSH_TS_SUSPENDED = 13,
	ADRENO_DEVICE_CORESIGHT_CX = 14,
	/** @ADRENO_DEVICE_DMS: Set if DMS is enabled */
	ADRENO_DEVICE_DMS = 15,
};

/**
 * struct adreno_drawobj_profile_entry - a single drawobj entry in the
 * kernel profiling buffer
 * @started: Number of GPU ticks at start of the drawobj
 * @retired: Number of GPU ticks at the end of the drawobj
 * @ctx_start: CP_ALWAYS_ON_CONTEXT tick at start of the drawobj
 * @ctx_end: CP_ALWAYS_ON_CONTEXT tick at end of the drawobj
 */
struct adreno_drawobj_profile_entry {
	uint64_t started;
	uint64_t retired;
	uint64_t ctx_start;
	uint64_t ctx_end;
};

#define ADRENO_DRAWOBJ_PROFILE_OFFSET(_index, _member) \
	 ((_index) * sizeof(struct adreno_drawobj_profile_entry) \
	  + offsetof(struct adreno_drawobj_profile_entry, _member))


/**
 * adreno_regs: List of registers that are used in kgsl driver for all
 * 3D devices. Each device type has different offset value for the same
 * register, so an array of register offsets are declared for every device
 * and are indexed by the enumeration values defined in this enum
 */
enum adreno_regs {
	ADRENO_REG_CP_ME_RAM_DATA,
	ADRENO_REG_CP_RB_BASE,
	ADRENO_REG_CP_RB_BASE_HI,
	ADRENO_REG_CP_RB_RPTR_ADDR_LO,
	ADRENO_REG_CP_RB_RPTR_ADDR_HI,
	ADRENO_REG_CP_RB_RPTR,
	ADRENO_REG_CP_RB_WPTR,
	ADRENO_REG_CP_ME_CNTL,
	ADRENO_REG_CP_RB_CNTL,
	ADRENO_REG_CP_IB1_BASE,
	ADRENO_REG_CP_IB1_BASE_HI,
	ADRENO_REG_CP_IB1_BUFSZ,
	ADRENO_REG_CP_IB2_BASE,
	ADRENO_REG_CP_IB2_BASE_HI,
	ADRENO_REG_CP_IB2_BUFSZ,
	ADRENO_REG_CP_TIMESTAMP,
	ADRENO_REG_CP_SCRATCH_REG6,
	ADRENO_REG_CP_SCRATCH_REG7,
	ADRENO_REG_CP_PROTECT_STATUS,
	ADRENO_REG_CP_PREEMPT,
	ADRENO_REG_CP_PREEMPT_DEBUG,
	ADRENO_REG_CP_PREEMPT_DISABLE,
	ADRENO_REG_CP_PROTECT_REG_0,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
	ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
	ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
	ADRENO_REG_CP_PREEMPT_LEVEL_STATUS,
	ADRENO_REG_RBBM_STATUS,
	ADRENO_REG_RBBM_STATUS3,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
	ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
	ADRENO_REG_RBBM_PERFCTR_PWR_1_LO,
	ADRENO_REG_RBBM_INT_0_MASK,
	ADRENO_REG_RBBM_PM_OVERRIDE2,
	ADRENO_REG_RBBM_SW_RESET_CMD,
	ADRENO_REG_RBBM_CLOCK_CTL,
	ADRENO_REG_PA_SC_AA_CONFIG,
	ADRENO_REG_SQ_GPR_MANAGEMENT,
	ADRENO_REG_SQ_INST_STORE_MANAGEMENT,
	ADRENO_REG_TP0_CHICKEN,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
	ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
	ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
	ADRENO_REG_GMU_AHB_FENCE_STATUS,
	ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
	ADRENO_REG_GPMU_POWER_COUNTER_ENABLE,
	ADRENO_REG_REGISTER_MAX,
};

#define ADRENO_REG_UNUSED	0xFFFFFFFF
#define ADRENO_REG_SKIP	0xFFFFFFFE
#define ADRENO_REG_DEFINE(_offset, _reg)[_offset] = _reg

struct adreno_irq_funcs {
	void (*func)(struct adreno_device *adreno_dev, int mask);
};
#define ADRENO_IRQ_CALLBACK(_c) { .func = _c }

/*
 * struct adreno_debugbus_block - Holds info about debug buses of a chip
 * @block_id: Bus identifier
 * @dwords: Number of dwords of data that this block holds
 */
struct adreno_debugbus_block {
	unsigned int block_id;
	unsigned int dwords;
};

enum adreno_cp_marker_type {
	IFPC_DISABLE,
	IFPC_ENABLE,
	IB1LIST_START,
	IB1LIST_END,
};

struct adreno_gpudev {
	/*
	 * These registers are in a different location on different devices,
	 * so define them in the structure and use them as variables.
	 */
	unsigned int *const reg_offsets;

	struct adreno_coresight *coresight[2];

	/* GPU specific function hooks */
	int (*probe)(struct platform_device *pdev, u32 chipid,
		const struct adreno_gpu_core *gpucore);
	void (*snapshot)(struct adreno_device *adreno_dev,
				struct kgsl_snapshot *snapshot);
	irqreturn_t (*irq_handler)(struct adreno_device *adreno_dev);
	int (*init)(struct adreno_device *adreno_dev);
	void (*remove)(struct adreno_device *adreno_dev);
	int (*rb_start)(struct adreno_device *adreno_dev);
	int (*start)(struct adreno_device *adreno_dev);
	int (*regulator_enable)(struct adreno_device *adreno_dev);
	void (*regulator_disable)(struct adreno_device *adreno_dev);
	void (*pwrlevel_change_settings)(struct adreno_device *adreno_dev,
				unsigned int prelevel, unsigned int postlevel,
				bool post);
	void (*preemption_schedule)(struct adreno_device *adreno_dev);
	int (*preemption_context_init)(struct kgsl_context *context);
	void (*context_detach)(struct adreno_context *drawctxt);
	void (*clk_set_options)(struct adreno_device *adreno_dev,
				const char *name, struct clk *clk, bool on);
	void (*pre_reset)(struct adreno_device *adreno_dev);
	void (*gpu_keepalive)(struct adreno_device *adreno_dev,
			bool state);
	bool (*hw_isidle)(struct adreno_device *adreno_dev);
	const char *(*iommu_fault_block)(struct kgsl_device *device,
				unsigned int fsynr1);
	int (*reset)(struct adreno_device *adreno_dev);
	/** @read_alwayson: Return the current value of the alwayson counter */
	u64 (*read_alwayson)(struct adreno_device *adreno_dev);
	/**
	 * @power_ops: Target specific function pointers to power up/down the
	 * gpu
	 */
	const struct adreno_power_ops *power_ops;
	int (*clear_pending_transactions)(struct adreno_device *adreno_dev);
	void (*deassert_gbif_halt)(struct adreno_device *adreno_dev);
	int (*ringbuffer_submitcmd)(struct adreno_device *adreno_dev,
			struct kgsl_drawobj_cmd *cmdobj, u32 flags,
			struct adreno_submit_time *time);
	/**
	 * @is_hw_collapsible: Return true if the hardware can be collapsed.
	 * Only used by non GMU/RGMU targets
	 */
	bool (*is_hw_collapsible)(struct adreno_device *adreno_dev);
	/**
	 * @power_stats - Return the perfcounter statistics for DCVS
	 */
	void (*power_stats)(struct adreno_device *adreno_dev,
			struct kgsl_power_stats *stats);
	int (*setproperty)(struct kgsl_device_private *priv, u32 type,
		void __user *value, u32 sizebytes);
	int (*add_to_va_minidump)(struct adreno_device *adreno_dev);
	/**
	 * @gx_is_on - Return true if both gfx clock and gxgdsc are enabled.
	 */
	bool (*gx_is_on)(struct adreno_device *adreno_dev);
	/**
	 * @send_recurring_cmdobj - Target specific function to send recurring IBs to GMU
	 */
	int (*send_recurring_cmdobj)(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj);
	/**
	 * @context_destroy: Target specific function called during context destruction
	 */
	void (*context_destroy)(struct adreno_device *adreno_dev, struct adreno_context *drawctxt);
};

/**
 * enum kgsl_ft_policy_bits - KGSL fault tolerance policy bits
 * @KGSL_FT_OFF: Disable fault detection (not used)
 * @KGSL_FT_REPLAY: Replay the faulting command
 * @KGSL_FT_SKIPIB: Skip the faulting indirect buffer
 * @KGSL_FT_SKIPFRAME: Skip the frame containing the faulting IB
 * @KGSL_FT_DISABLE: Tells the dispatcher to disable FT for the command obj
 * @KGSL_FT_TEMP_DISABLE: Disables FT for all commands
 * @KGSL_FT_THROTTLE: Disable the context if it faults too often
 * @KGSL_FT_SKIPCMD: Skip the command containing the faulting IB
 */
enum kgsl_ft_policy_bits {
	KGSL_FT_OFF = 0,
	KGSL_FT_REPLAY,
	KGSL_FT_SKIPIB,
	KGSL_FT_SKIPFRAME,
	KGSL_FT_DISABLE,
	KGSL_FT_TEMP_DISABLE,
	KGSL_FT_THROTTLE,
	KGSL_FT_SKIPCMD,
	/* KGSL_FT_MAX_BITS is used to calculate the mask */
	KGSL_FT_MAX_BITS,
	/* Internal bits - set during GFT */
	/* Skip the PM dump on replayed command obj's */
	KGSL_FT_SKIP_PMDUMP = 31,
};

#define KGSL_FT_POLICY_MASK GENMASK(KGSL_FT_MAX_BITS - 1, 0)

#define FOR_EACH_RINGBUFFER(_dev, _rb, _i)			\
	for ((_i) = 0, (_rb) = &((_dev)->ringbuffers[0]);	\
		(_i) < (_dev)->num_ringbuffers;			\
		(_i)++, (_rb)++)

extern const struct adreno_power_ops adreno_power_operations;

extern const struct adreno_gpudev adreno_a3xx_gpudev;
extern const struct adreno_gpudev adreno_a5xx_gpudev;
extern const struct adreno_gpudev adreno_a6xx_gpudev;
extern const struct adreno_gpudev adreno_a6xx_rgmu_gpudev;
extern const struct adreno_gpudev adreno_a619_holi_gpudev;

extern int adreno_wake_nice;
extern unsigned int adreno_wake_timeout;

int adreno_start(struct kgsl_device *device, int priority);
long adreno_ioctl(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg);

long adreno_ioctl_helper(struct kgsl_device_private *dev_priv,
		unsigned int cmd, unsigned long arg,
		const struct kgsl_ioctl *cmds, int len);

/*
 * adreno_switch_to_unsecure_mode - Execute a zap shader
 * @adreno_dev: An Adreno GPU handle
 * @rb: The ringbuffer to execute on
 *
 * Execute the zap shader from the CP to take the GPU out of secure mode.
 * Return: 0 on success or negative on failure
 */
int adreno_switch_to_unsecure_mode(struct adreno_device *adreno_dev,
				struct adreno_ringbuffer *rb);

int adreno_spin_idle(struct adreno_device *device, unsigned int timeout);
int adreno_idle(struct kgsl_device *device);

void adreno_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot,
		struct kgsl_context *context, struct kgsl_context *context_lpac);

int adreno_reset(struct kgsl_device *device, int fault);

void adreno_fault_skipcmd_detached(struct adreno_device *adreno_dev,
					 struct adreno_context *drawctxt,
					 struct kgsl_drawobj *drawobj);

void adreno_hang_int_callback(struct adreno_device *adreno_dev, int bit);
void adreno_cp_callback(struct adreno_device *adreno_dev, int bit);

int adreno_sysfs_init(struct adreno_device *adreno_dev);

void adreno_irqctrl(struct adreno_device *adreno_dev, int state);

long adreno_ioctl_perfcounter_get(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

long adreno_ioctl_perfcounter_put(struct kgsl_device_private *dev_priv,
	unsigned int cmd, void *data);

bool adreno_is_cx_dbgc_register(struct kgsl_device *device,
		unsigned int offset);
void adreno_cx_dbgc_regread(struct kgsl_device *adreno_device,
		unsigned int offsetwords, unsigned int *value);
void adreno_cx_dbgc_regwrite(struct kgsl_device *device,
		unsigned int offsetwords, unsigned int value);
void adreno_cx_misc_regread(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int *value);
void adreno_cx_misc_regwrite(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int value);
void adreno_cx_misc_regrmw(struct adreno_device *adreno_dev,
		unsigned int offsetwords,
		unsigned int mask, unsigned int bits);
void adreno_isense_regread(struct adreno_device *adreno_dev,
		unsigned int offsetwords, unsigned int *value);
bool adreno_gx_is_on(struct adreno_device *adreno_dev);

/**
 * adreno_active_count_get - Wrapper for target specific active count get
 * @adreno_dev: pointer to the adreno device
 *
 * Increase the active count for the KGSL device and execute slumber exit
 * sequence if this is the first reference. Code paths that need to touch the
 * hardware or wait for the hardware to complete an operation must hold an
 * active count reference until they are finished. The device mutex must be held
 * while calling this function.
 *
 * Return: 0 on success or negative error on failure to wake up the device
 */
int adreno_active_count_get(struct adreno_device *adreno_dev);

/**
 * adreno_active_count_put - Wrapper for target specific active count put
 * @adreno_dev: pointer to the adreno device
 *
 * Decrease the active or the KGSL device and schedule the idle thread to
 * execute the slumber sequence if there are no remaining references. The
 * device mutex must be held while calling this function.
 */
void adreno_active_count_put(struct adreno_device *adreno_dev);

#define ADRENO_TARGET(_name, _id) \
static inline int adreno_is_##_name(struct adreno_device *adreno_dev) \
{ \
	return (ADRENO_GPUREV(adreno_dev) == (_id)); \
}

static inline int adreno_is_a3xx(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) >= 300) &&
		(ADRENO_GPUREV(adreno_dev) < 400));
}

ADRENO_TARGET(a304, ADRENO_REV_A304)
ADRENO_TARGET(a306, ADRENO_REV_A306)
ADRENO_TARGET(a306a, ADRENO_REV_A306A)

static inline int adreno_is_a5xx(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 500 &&
			ADRENO_GPUREV(adreno_dev) < 600;
}

ADRENO_TARGET(a505, ADRENO_REV_A505)
ADRENO_TARGET(a506, ADRENO_REV_A506)
ADRENO_TARGET(a508, ADRENO_REV_A508)
ADRENO_TARGET(a510, ADRENO_REV_A510)
ADRENO_TARGET(a512, ADRENO_REV_A512)
ADRENO_TARGET(a530, ADRENO_REV_A530)
ADRENO_TARGET(a540, ADRENO_REV_A540)

static inline int adreno_is_a530v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1);
}

static inline int adreno_is_a530v3(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A530) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 2);
}

static inline int adreno_is_a505_or_a506(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 505 &&
			ADRENO_GPUREV(adreno_dev) <= 506;
}

static inline int adreno_is_a6xx(struct adreno_device *adreno_dev)
{
	return ((ADRENO_GPUREV(adreno_dev) >= 600 &&
			ADRENO_GPUREV(adreno_dev) < 700) ||
			ADRENO_GPUREV(adreno_dev) == 0x032600);
}

static inline int adreno_is_a660_shima(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A660) &&
		(adreno_dev->gpucore->compatible &&
		!strcmp(adreno_dev->gpucore->compatible,
		"qcom,adreno-gpu-a660-shima"));
}

ADRENO_TARGET(a610, ADRENO_REV_A610)
ADRENO_TARGET(a612, ADRENO_REV_A612)
ADRENO_TARGET(a618, ADRENO_REV_A618)
ADRENO_TARGET(a619, ADRENO_REV_A619)
ADRENO_TARGET(a621, ADRENO_REV_A621)
ADRENO_TARGET(a630, ADRENO_REV_A630)
ADRENO_TARGET(a635, ADRENO_REV_A635)
ADRENO_TARGET(a662, ADRENO_REV_A662)
ADRENO_TARGET(a640, ADRENO_REV_A640)
ADRENO_TARGET(a650, ADRENO_REV_A650)
ADRENO_TARGET(a680, ADRENO_REV_A680)
ADRENO_TARGET(gen6_3_26_0, ADRENO_REV_GEN6_3_26_0)

/* A635 is derived from A660 and shares same logic */
static inline int adreno_is_a660(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A660 || rev == ADRENO_REV_A635 ||
			rev == ADRENO_REV_A662);
}

/*
 * All the derived chipsets from A615 needs to be added to this
 * list such as A616, A618, A619 etc.
 */
static inline int adreno_is_a615_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A615 || rev == ADRENO_REV_A616 ||
			rev == ADRENO_REV_A618 || rev == ADRENO_REV_A619);
}

/*
 * Derived GPUs from A640 needs to be added to this list.
 * A640 and A680 belongs to this family.
 */
static inline int adreno_is_a640_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A640 || rev == ADRENO_REV_A680);
}

/*
 * Derived GPUs from A650 needs to be added to this list.
 * A650 is derived from A640 but register specs has been
 * changed hence do not belongs to A640 family. A620, A621,
 * A660, A690 follows the register specs of A650.
 *
 */
static inline int adreno_is_a650_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A650 || rev == ADRENO_REV_A620 ||
		rev == ADRENO_REV_A660 || rev == ADRENO_REV_A635 ||
		rev == ADRENO_REV_A662 ||  rev == ADRENO_REV_A621);
}

static inline int adreno_is_a619_holi(struct adreno_device *adreno_dev)
{
	return of_device_is_compatible(adreno_dev->dev.pdev->dev.of_node,
		"qcom,adreno-gpu-a619-holi");
}

static inline int adreno_is_a620(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A620 || rev == ADRENO_REV_A621);
}

static inline int adreno_is_a612_family(struct adreno_device *adreno_dev)
{
	unsigned int rev = ADRENO_GPUREV(adreno_dev);

	return (rev == ADRENO_REV_A612 || rev == ADRENO_REV_GEN6_3_26_0);
}

static inline int adreno_is_a640v2(struct adreno_device *adreno_dev)
{
	return (ADRENO_GPUREV(adreno_dev) == ADRENO_REV_A640) &&
		(ADRENO_CHIPID_PATCH(adreno_dev->chipid) == 1);
}

static inline int adreno_is_gen7(struct adreno_device *adreno_dev)
{
	return ADRENO_GPUREV(adreno_dev) >= 0x070000 &&
			ADRENO_GPUREV(adreno_dev) < 0x080000;
}

ADRENO_TARGET(gen7_0_0, ADRENO_REV_GEN7_0_0)
ADRENO_TARGET(gen7_0_1, ADRENO_REV_GEN7_0_1)
ADRENO_TARGET(gen7_4_0, ADRENO_REV_GEN7_4_0)
ADRENO_TARGET(gen7_3_0, ADRENO_REV_GEN7_3_0)
ADRENO_TARGET(gen7_6_0, ADRENO_REV_GEN7_6_0)

static inline int adreno_is_gen7_0_x_family(struct adreno_device *adreno_dev)
{
	return adreno_is_gen7_0_0(adreno_dev) || adreno_is_gen7_0_1(adreno_dev) ||
		adreno_is_gen7_4_0(adreno_dev) || adreno_is_gen7_3_0(adreno_dev);
}

/*
 * adreno_checkreg_off() - Checks the validity of a register enum
 * @adreno_dev:		Pointer to adreno device
 * @offset_name:	The register enum that is checked
 */
static inline bool adreno_checkreg_off(struct adreno_device *adreno_dev,
					enum adreno_regs offset_name)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (offset_name >= ADRENO_REG_REGISTER_MAX ||
		gpudev->reg_offsets[offset_name] == ADRENO_REG_UNUSED)
		return false;

	/*
	 * GPU register programming is kept common as much as possible
	 * across the cores, Use ADRENO_REG_SKIP when certain register
	 * programming needs to be skipped for certain GPU cores.
	 * Example: Certain registers on a5xx like IB1_BASE are 64 bit.
	 * Common programming programs 64bit register but upper 32 bits
	 * are skipped in a3xx using ADRENO_REG_SKIP.
	 */
	if (gpudev->reg_offsets[offset_name] == ADRENO_REG_SKIP)
		return false;

	return true;
}

/*
 * adreno_readreg() - Read a register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be read
 * @val:		Register value read is placed here
 */
static inline void adreno_readreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int *val)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regread(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets[offset_name], val);
	else
		*val = 0;
}

/*
 * adreno_writereg() - Write a register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be written
 * @val:		Value to write
 */
static inline void adreno_writereg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int val)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, offset_name))
		kgsl_regwrite(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets[offset_name], val);
}

/*
 * adreno_getreg() - Returns the offset value of a register from the
 * register offset array in the gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum whore offset is returned
 */
static inline unsigned int adreno_getreg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (!adreno_checkreg_off(adreno_dev, offset_name))
		return ADRENO_REG_REGISTER_MAX;
	return gpudev->reg_offsets[offset_name];
}

/*
 * adreno_write_gmureg() - Write a GMU register by getting its offset from the
 * offset array defined in gpudev node
 * @adreno_dev:		Pointer to the the adreno device
 * @offset_name:	The register enum that is to be written
 * @val:		Value to write
 */
static inline void adreno_write_gmureg(struct adreno_device *adreno_dev,
				enum adreno_regs offset_name, unsigned int val)
{
	const struct adreno_gpudev *gpudev = ADRENO_GPU_DEVICE(adreno_dev);

	if (adreno_checkreg_off(adreno_dev, offset_name))
		gmu_core_regwrite(KGSL_DEVICE(adreno_dev),
				gpudev->reg_offsets[offset_name], val);
}

/**
 * adreno_gpu_fault() - Return the current state of the GPU
 * @adreno_dev: A pointer to the adreno_device to query
 *
 * Return 0 if there is no fault or positive with the last type of fault that
 * occurred
 */
static inline unsigned int adreno_gpu_fault(struct adreno_device *adreno_dev)
{
	/* make sure we're reading the latest value */
	smp_rmb();
	return atomic_read(&adreno_dev->dispatcher.fault);
}

/**
 * adreno_set_gpu_fault() - Set the current fault status of the GPU
 * @adreno_dev: A pointer to the adreno_device to set
 * @state: fault state to set
 *
 */
static inline void adreno_set_gpu_fault(struct adreno_device *adreno_dev,
	int state)
{
	/* only set the fault bit w/o overwriting other bits */
	atomic_or(state, &adreno_dev->dispatcher.fault);

	/* make sure other CPUs see the update */
	smp_wmb();
}

static inline bool adreno_gmu_gpu_fault(struct adreno_device *adreno_dev)
{
	return adreno_gpu_fault(adreno_dev) & ADRENO_GMU_FAULT;
}

/**
 * adreno_clear_gpu_fault() - Clear the GPU fault register
 * @adreno_dev: A pointer to an adreno_device structure
 *
 * Clear the GPU fault status for the adreno device
 */

static inline void adreno_clear_gpu_fault(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->dispatcher.fault, 0);

	/* make sure other CPUs see the update */
	smp_wmb();
}

/**
 * adreno_gpu_halt() - Return the GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline int adreno_gpu_halt(struct adreno_device *adreno_dev)
{
	/* make sure we're reading the latest value */
	smp_rmb();
	return atomic_read(&adreno_dev->halt);
}


/**
 * adreno_clear_gpu_halt() - Clear the GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_clear_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_set(&adreno_dev->halt, 0);

	/* make sure other CPUs see the update */
	smp_wmb();
}

/**
 * adreno_get_gpu_halt() - Increment GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_get_gpu_halt(struct adreno_device *adreno_dev)
{
	atomic_inc(&adreno_dev->halt);
}

/**
 * adreno_put_gpu_halt() - Decrement GPU halt refcount
 * @adreno_dev: A pointer to the adreno_device
 */
static inline void adreno_put_gpu_halt(struct adreno_device *adreno_dev)
{
	/* Make sure the refcount is good */
	int ret = atomic_dec_if_positive(&adreno_dev->halt);

	WARN(ret < 0, "GPU halt refcount unbalanced\n");
}


#ifdef CONFIG_DEBUG_FS
void adreno_debugfs_init(struct adreno_device *adreno_dev);
void adreno_context_debugfs_init(struct adreno_device *adreno_dev,
				struct adreno_context *ctx);
#else
static inline void adreno_debugfs_init(struct adreno_device *adreno_dev) { }
static inline void adreno_context_debugfs_init(struct adreno_device *device,
						struct adreno_context *context)
{
	context->debug_root = NULL;
}
#endif

/**
 * adreno_compare_pm4_version() - Compare the PM4 microcode version
 * @adreno_dev: Pointer to the adreno_device struct
 * @version: Version number to compare again
 *
 * Compare the current version against the specified version and return -1 if
 * the current code is older, 0 if equal or 1 if newer.
 */
static inline int adreno_compare_pm4_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->fw[ADRENO_FW_PM4].version == version)
		return 0;

	return (adreno_dev->fw[ADRENO_FW_PM4].version > version) ? 1 : -1;
}

/**
 * adreno_compare_pfp_version() - Compare the PFP microcode version
 * @adreno_dev: Pointer to the adreno_device struct
 * @version: Version number to compare against
 *
 * Compare the current version against the specified version and return -1 if
 * the current code is older, 0 if equal or 1 if newer.
 */
static inline int adreno_compare_pfp_version(struct adreno_device *adreno_dev,
	unsigned int version)
{
	if (adreno_dev->fw[ADRENO_FW_PFP].version == version)
		return 0;

	return (adreno_dev->fw[ADRENO_FW_PFP].version > version) ? 1 : -1;
}

/**
 * adreno_in_preempt_state() - Check if preemption state is equal to given state
 * @adreno_dev: Device whose preemption state is checked
 * @state: State to compare against
 */
static inline bool adreno_in_preempt_state(struct adreno_device *adreno_dev,
			enum adreno_preempt_states state)
{
	return atomic_read(&adreno_dev->preempt.state) == state;
}
/**
 * adreno_set_preempt_state() - Set the specified preemption state
 * @adreno_dev: Device to change preemption state
 * @state: State to set
 */
static inline void adreno_set_preempt_state(struct adreno_device *adreno_dev,
		enum adreno_preempt_states state)
{
	/*
	 * atomic_set doesn't use barriers, so we need to do it ourselves.  One
	 * before...
	 */
	smp_wmb();
	atomic_set(&adreno_dev->preempt.state, state);

	/* ... and one after */
	smp_wmb();
}

static inline bool adreno_is_preemption_enabled(
				struct adreno_device *adreno_dev)
{
	return test_bit(ADRENO_DEVICE_PREEMPTION, &adreno_dev->priv);
}

/*
 * adreno_compare_prio_level() - Compares 2 priority levels based on enum values
 * @p1: First priority level
 * @p2: Second priority level
 *
 * Returns greater than 0 if p1 is higher priority, 0 if levels are equal else
 * less than 0
 */
static inline int adreno_compare_prio_level(int p1, int p2)
{
	return p2 - p1;
}

void adreno_readreg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t *val);

void adreno_writereg64(struct adreno_device *adreno_dev,
		enum adreno_regs lo, enum adreno_regs hi, uint64_t val);

unsigned int adreno_get_rptr(struct adreno_ringbuffer *rb);

static inline bool adreno_rb_empty(struct adreno_ringbuffer *rb)
{
	return (adreno_get_rptr(rb) == rb->wptr);
}

static inline bool adreno_soft_fault_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->fast_hang_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

static inline bool adreno_long_ib_detect(struct adreno_device *adreno_dev)
{
	return adreno_dev->long_ib_detect &&
		!test_bit(ADRENO_DEVICE_ISDB_ENABLED, &adreno_dev->priv);
}

/**
 * adreno_support_64bit - Return true if the GPU supports 64 bit addressing
 * @adreno_dev: An Adreno GPU device handle
 *
 * Return: True if the device supports 64 bit addressing
 */
static inline bool adreno_support_64bit(struct adreno_device *adreno_dev)
{
	/*
	 * The IOMMU API takes a unsigned long for the iova so we can't support
	 * 64 bit addresses when the kernel is in 32 bit mode even if we wanted
	 * so we need to check that we are using a5xx or newer and that the
	 * unsigned long is big enough for our purposes.
	 */
	return (BITS_PER_LONG > 32 && ADRENO_GPUREV(adreno_dev) >= 500);
}

static inline void adreno_ringbuffer_set_pagetable(struct kgsl_device *device,
	struct adreno_ringbuffer *rb, struct kgsl_pagetable *pt)
{
	unsigned long flags;

	spin_lock_irqsave(&rb->preempt_lock, flags);

	kgsl_sharedmem_writel(device->scratch,
		SCRATCH_RB_OFFSET(rb->id, current_rb_ptname), pt->name);

	kgsl_sharedmem_writeq(device->scratch,
		SCRATCH_RB_OFFSET(rb->id, ttbr0),
		kgsl_mmu_pagetable_get_ttbr0(pt));

	kgsl_sharedmem_writel(device->scratch,
		SCRATCH_RB_OFFSET(rb->id, contextidr), 0);

	spin_unlock_irqrestore(&rb->preempt_lock, flags);
}

static inline u32 counter_delta(struct kgsl_device *device,
			unsigned int reg, unsigned int *counter)
{
	u32 val, ret = 0;

	if (!reg)
		return 0;

	kgsl_regread(device, reg, &val);

	if (*counter) {
		if (val >= *counter)
			ret = val - *counter;
		else
			ret = (UINT_MAX - *counter) + val;
	}

	*counter = val;
	return ret;
}

static inline int adreno_perfcntr_active_oob_get(
	struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = adreno_active_count_get(adreno_dev);

	if (!ret) {
		ret = gmu_core_dev_oob_set(device, oob_perfcntr);
		if (ret)
			adreno_active_count_put(adreno_dev);
	}

	return ret;
}

static inline void adreno_perfcntr_active_oob_put(
	struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	gmu_core_dev_oob_clear(device, oob_perfcntr);
	adreno_active_count_put(adreno_dev);
}

/**
 * adreno_wait_for_halt_ack - wait for acknowlegement for a bus halt request
 * @ack_reg: register offset to wait for acknowledge
 * @mask: A mask value to wait for
 *
 * Return: 0 on success or -ETIMEDOUT if the request timed out
 */
static inline int adreno_wait_for_halt_ack(struct kgsl_device *device,
	int ack_reg, unsigned int mask)
{
	u32 val;
	int ret = kgsl_regmap_read_poll_timeout(&device->regmap, ack_reg,
		val, (val & mask) == mask, 100, 100 * 1000);

	if (ret)
		dev_err(device->dev,
			"GBIF/VBIF Halt ack timeout: reg=%08x mask=%08x status=%08x\n",
			ack_reg, mask, val);

	return ret;
}

/**
 * adreno_move_preempt_state - Update the preemption state
 * @adreno_dev: An Adreno GPU device handle
 * @old: The current state of the preemption
 * @new: The new state of the preemption
 *
 * Return: True if the state was updated or false if not
 */
static inline bool adreno_move_preempt_state(struct adreno_device *adreno_dev,
	enum adreno_preempt_states old, enum adreno_preempt_states new)
{
	return (atomic_cmpxchg(&adreno_dev->preempt.state, old, new) == old);
}

/**
 * adreno_reg_offset_init - Helper function to initialize reg_offsets
 * @reg_offsets: Pointer to an array of register offsets
 *
 * Helper function to setup register_offsets for a target. Go through
 * and set ADRENO_REG_UNUSED for all unused entries in the list.
 */
static inline void adreno_reg_offset_init(u32 *reg_offsets)
{
	int i;

	/*
	 * Initialize uninitialzed gpu registers, only needs to be done once.
	 * Make all offsets that are not initialized to ADRENO_REG_UNUSED
	 */
	for (i = 0; i < ADRENO_REG_REGISTER_MAX; i++) {
		if (!reg_offsets[i])
			reg_offsets[i] = ADRENO_REG_UNUSED;
	}
}

static inline u32 adreno_get_level(struct kgsl_context *context)
{
	u32 level;

	if (kgsl_context_is_lpac(context))
		return KGSL_LPAC_RB_ID;

	level = context->priority / KGSL_PRIORITY_MAX_RB_LEVELS;

	return min_t(u32, level, KGSL_PRIORITY_MAX_RB_LEVELS - 1);
}


/**
 * adreno_get_firwmare - Load firmware into a adreno_firmware struct
 * @adreno_dev: An Adreno GPU device handle
 * @fwfile: Firmware file to load
 * @firmware: A &struct adreno_firmware container for the firmware.
 *
 * Load the specified firmware file into the memdesc in &struct adreno_firmware
 * and get the size and version from the data.
 *
 * Return: 0 on success or negative on failure
 */
int adreno_get_firmware(struct adreno_device *adreno_dev,
		const char *fwfile, struct adreno_firmware *firmware);
/**
 * adreno_zap_shader_load - Helper function for loading the zap shader
 * adreno_dev: A handle to an Adreno GPU device
 * name: Name of the zap shader to load
 *
 * A target indepedent helper function for loading the zap shader.
 *
 * Return: 0 on success or negative on failure.
 */
int adreno_zap_shader_load(struct adreno_device *adreno_dev,
		const char *name);

/**
 * adreno_irq_callbacks - Helper function to handle IRQ callbacks
 * @adreno_dev: Adreno GPU device handle
 * @funcs: List of callback functions
 * @status: Interrupt status
 *
 * Walk the bits in the interrupt status and call any applicable callbacks.
 * Return: IRQ_HANDLED if one or more interrupt callbacks were called.
 */
irqreturn_t adreno_irq_callbacks(struct adreno_device *adreno_dev,
		const struct adreno_irq_funcs *funcs, u32 status);


/**
 * adreno_device_probe - Generic adreno device probe function
 * @pdev: Pointer to the platform device
 * @adreno_dev: Adreno GPU device handle
 *
 * Do the generic setup for the Adreno device. Called from the target specific
 * probe functions.
 *
 * Return: 0 on success or negative on failure
 */
int adreno_device_probe(struct platform_device *pdev,
		struct adreno_device *adreno_dev);

/**
 * adreno_power_cycle - Suspend and resume the device
 * @adreno_dev: Pointer to the adreno device
 * @callback: Function that needs to be executed
 * @priv: Argument to be passed to the callback
 *
 * Certain properties that can be set via sysfs need to power
 * cycle the device to take effect. This function suspends
 * the device, executes the callback, and resumes the device.
 *
 * Return: 0 on success or negative on failure
 */
int adreno_power_cycle(struct adreno_device *adreno_dev,
	void (*callback)(struct adreno_device *adreno_dev, void *priv),
	void *priv);

/**
 * adreno_power_cycle_bool - Power cycle the device to change device setting
 * @adreno_dev: Pointer to the adreno device
 * @flag: Flag that needs to be set
 * @val: The value flag should be set to
 *
 * Certain properties that can be set via sysfs need to power cycle the device
 * to take effect. This function suspends the device, sets the flag, and
 * resumes the device.
 *
 * Return: 0 on success or negative on failure
 */
int adreno_power_cycle_bool(struct adreno_device *adreno_dev,
	bool *flag, bool val);

/**
 * adreno_power_cycle_u32 - Power cycle the device to change device setting
 * @adreno_dev: Pointer to the adreno device
 * @flag: Flag that needs to be set
 * @val: The value flag should be set to
 *
 * Certain properties that can be set via sysfs need to power cycle the device
 * to take effect. This function suspends the device, sets the flag, and
 * resumes the device.
 *
 * Return: 0 on success or negative on failure
 */
int adreno_power_cycle_u32(struct adreno_device *adreno_dev,
	u32 *flag, u32 val);

/**
 * adreno_set_active_ctxs_null - Give up active context refcount
 * @adreno_dev: Adreno GPU device handle
 *
 * This puts back the reference for that last active context on
 * each ringbuffer when going in and out of slumber.
 */
void adreno_set_active_ctxs_null(struct adreno_device *adreno_dev);

/**
 * adreno_get_bus_counters - Allocate the bus dcvs counters
 * @adreno_dev: Adreno GPU device handle
 *
 * This function allocates the various gpu counters to measure
 * gpu bus usage for bus dcvs
 */
void adreno_get_bus_counters(struct adreno_device *adreno_dev);

/**
 * adreno_suspend_context - Make sure device is idle
 * @device: Pointer to the kgsl device
 *
 * This function processes the profiling results and checks if the
 * device is idle so that it can be turned off safely
 *
 * Return: 0 on success or negative error on failure
 */
int adreno_suspend_context(struct kgsl_device *device);

/*
 * adreno_profile_submit_time - Populate profiling buffer with timestamps
 * @time: Container for the statistics
 *
 * Populate the draw object user profiling buffer with the timestamps
 * recored in the adreno_submit_time structure at the time of draw object
 * submission.
 */
void adreno_profile_submit_time(struct adreno_submit_time *time);

void adreno_preemption_timer(struct timer_list *t);

/**
 * adreno_create_profile_buffer - Create a buffer to store profiling data
 * @adreno_dev: Adreno GPU device handle
 */
void adreno_create_profile_buffer(struct adreno_device *adreno_dev);

/**
 * adreno_isidle - return true if the hardware is idle
 * @adreno_dev: Adreno GPU device handle
 *
 * Return: True if the hardware is idle
 */
bool adreno_isidle(struct adreno_device *adreno_dev);

/**
 * adreno_allocate_global - Helper function to allocate a global GPU object
 * @device: A GPU device handle
 * @memdesc: Pointer to a &struct kgsl_memdesc pointer
 * @size: Size of the allocation in bytes
 * @padding: Amount of extra adding to add to the VA allocation
 * @flags: Control flags for the allocation
 * @priv: Internal flags for the allocation
 * @name: Name of the allocation (for the debugfs file)
 *
 * Allocate a global object if it hasn't already been alllocated and put it in
 * the pointer pointed to by @memdesc.
 * Return: 0 on success or negative on error
 */
static inline int adreno_allocate_global(struct kgsl_device *device,
		struct kgsl_memdesc **memdesc, u64 size, u32 padding, u64 flags,
		u32 priv, const char *name)
{
	if (!IS_ERR_OR_NULL(*memdesc))
		return 0;

	*memdesc = kgsl_allocate_global(device, size, padding, flags, priv, name);
	return PTR_ERR_OR_ZERO(*memdesc);
}

static inline void adreno_set_dispatch_ops(struct adreno_device *adreno_dev,
		const struct adreno_dispatch_ops *ops)
{
	adreno_dev->dispatch_ops = ops;
}

#ifdef CONFIG_QCOM_KGSL_FENCE_TRACE
/**
 * adreno_fence_trace_array_init - Initialize an always on trace array
 * @device: A GPU device handle
 *
 * Register an always-on trace array to for fence timeout debugging
 */
void adreno_fence_trace_array_init(struct kgsl_device *device);
#else
static inline void adreno_fence_trace_array_init(struct kgsl_device *device) {}
#endif

/**
 * adreno_get_gpu_model - Gets gpu model name from device tree (or) chipid
 * @device: A GPU device handle
 *
 * Return: GPU model name string
 */
const char *adreno_get_gpu_model(struct kgsl_device *device);

int adreno_verify_cmdobj(struct kgsl_device_private *dev_priv,
		struct kgsl_context *context, struct kgsl_drawobj *drawobj[],
		uint32_t count);
#endif /*__ADRENO_H */
