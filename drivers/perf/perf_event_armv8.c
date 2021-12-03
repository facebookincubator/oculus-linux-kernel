/*
 * PMU support
 *
 * Copyright (C) 2012 ARM Limited
 * Author: Will Deacon <will.deacon@arm.com>
 *
 * This code is based heavily on the ARMv7 perf event code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/irq_regs.h>
#include <asm/perf_event.h>

#include <linux/of.h>
#include <linux/perf/arm_pmu.h>
#include <linux/platform_device.h>

/*
 * ARMv8 PMUv3 Performance Events handling code.
 * Common event types.
 */
enum armv8_pmuv3_perf_types {
	/* Required events. */
	ARMV8_PMUV3_PERFCTR_PMNC_SW_INCR			= 0x00,
	ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL			= 0x03,
	ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS			= 0x04,
	ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED			= 0x10,
	ARMV8_PMUV3_PERFCTR_CLOCK_CYCLES			= 0x11,
	ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED			= 0x12,

	/* At least one of the following is required. */
	ARMV8_PMUV3_PERFCTR_INSTR_EXECUTED			= 0x08,
	ARMV8_PMUV3_PERFCTR_OP_SPEC				= 0x1B,

	/* Common architectural events. */
	ARMV8_PMUV3_PERFCTR_MEM_READ				= 0x06,
	ARMV8_PMUV3_PERFCTR_MEM_WRITE				= 0x07,
	ARMV8_PMUV3_PERFCTR_EXC_TAKEN				= 0x09,
	ARMV8_PMUV3_PERFCTR_EXC_EXECUTED			= 0x0A,
	ARMV8_PMUV3_PERFCTR_CID_WRITE				= 0x0B,
	ARMV8_PMUV3_PERFCTR_PC_WRITE				= 0x0C,
	ARMV8_PMUV3_PERFCTR_PC_IMM_BRANCH			= 0x0D,
	ARMV8_PMUV3_PERFCTR_PC_PROC_RETURN			= 0x0E,
	ARMV8_PMUV3_PERFCTR_MEM_UNALIGNED_ACCESS		= 0x0F,
	ARMV8_PMUV3_PERFCTR_TTBR_WRITE				= 0x1C,
	ARMV8_PMUV3_PERFCTR_BR_RETIRED				= 0x21,

	/* Common microarchitectural events. */
	ARMV8_PMUV3_PERFCTR_L1_ICACHE_REFILL			= 0x01,
	ARMV8_PMUV3_PERFCTR_ITLB_REFILL				= 0x02,
	ARMV8_PMUV3_PERFCTR_DTLB_REFILL				= 0x05,
	ARMV8_PMUV3_PERFCTR_MEM_ACCESS				= 0x13,
	ARMV8_PMUV3_PERFCTR_L1_ICACHE_ACCESS			= 0x14,
	ARMV8_PMUV3_PERFCTR_L1_DCACHE_WB			= 0x15,
	ARMV8_PMUV3_PERFCTR_L2_CACHE_ACCESS			= 0x16,
	ARMV8_PMUV3_PERFCTR_L2_CACHE_REFILL			= 0x17,
	ARMV8_PMUV3_PERFCTR_L2_CACHE_WB				= 0x18,
	ARMV8_PMUV3_PERFCTR_BUS_ACCESS				= 0x19,
	ARMV8_PMUV3_PERFCTR_MEM_ERROR				= 0x1A,
	ARMV8_PMUV3_PERFCTR_BUS_CYCLES				= 0x1D,
	ARMV8_PMUV3_PERFCTR_L1_DCACHE_ALLOCATE			= 0x1F,
	ARMV8_PMUV3_PERFCTR_L2_CACHE_ALLOCATE			= 0x20,
	ARMV8_PMUV3_PERFCTR_BR_MIS_PRED_RETIRED			= 0x22,
	ARMV8_PMUV3_PERFCTR_STALL_FRONTEND			= 0x23,
	ARMV8_PMUV3_PERFCTR_STALL_BACKEND			= 0x24,
};

/* ARMv8 Cortex-A53 specific event types. */
enum armv8_a53_pmu_perf_types {
	ARMV8_A53_PERFCTR_PREFETCH_LINEFILL			= 0xC2,
};

/* ARMv8 Cortex-A57 specific event types. */
enum armv8_a57_perf_types {
	ARMV8_A57_PERFCTR_L1_DCACHE_ACCESS_LD			= 0x40,
	ARMV8_A57_PERFCTR_L1_DCACHE_ACCESS_ST			= 0x41,
	ARMV8_A57_PERFCTR_L1_DCACHE_REFILL_LD			= 0x42,
	ARMV8_A57_PERFCTR_L1_DCACHE_REFILL_ST			= 0x43,
	ARMV8_A57_PERFCTR_DTLB_REFILL_LD			= 0x4c,
	ARMV8_A57_PERFCTR_DTLB_REFILL_ST			= 0x4d,
};

/* PMUv3 HW events mapping. */
const unsigned armv8_pmuv3_perf_map[PERF_COUNT_HW_MAX] = {
	PERF_MAP_ALL_UNSUPPORTED,
	[PERF_COUNT_HW_CPU_CYCLES]		= ARMV8_PMUV3_PERFCTR_CLOCK_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]		= ARMV8_PMUV3_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[PERF_COUNT_HW_CACHE_MISSES]		= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[PERF_COUNT_HW_BRANCH_MISSES]		= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]		= ARMV8_PMUV3_PERFCTR_BUS_CYCLES,
	[PERF_COUNT_HW_STALLED_CYCLES_FRONTEND]	= ARMV8_PMUV3_PERFCTR_STALL_FRONTEND,
	[PERF_COUNT_HW_STALLED_CYCLES_BACKEND]	= ARMV8_PMUV3_PERFCTR_STALL_BACKEND,
};

/* ARM Cortex-A53 HW events mapping. */
static const unsigned armv8_a53_perf_map[PERF_COUNT_HW_MAX] = {
	PERF_MAP_ALL_UNSUPPORTED,
	[PERF_COUNT_HW_CPU_CYCLES]		= ARMV8_PMUV3_PERFCTR_CLOCK_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]		= ARMV8_PMUV3_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[PERF_COUNT_HW_CACHE_MISSES]		= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[PERF_COUNT_HW_BRANCH_INSTRUCTIONS]	= ARMV8_PMUV3_PERFCTR_PC_WRITE,
	[PERF_COUNT_HW_BRANCH_MISSES]		= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]		= ARMV8_PMUV3_PERFCTR_BUS_CYCLES,
};

static const unsigned armv8_a57_perf_map[PERF_COUNT_HW_MAX] = {
	PERF_MAP_ALL_UNSUPPORTED,
	[PERF_COUNT_HW_CPU_CYCLES]		= ARMV8_PMUV3_PERFCTR_CLOCK_CYCLES,
	[PERF_COUNT_HW_INSTRUCTIONS]		= ARMV8_PMUV3_PERFCTR_INSTR_EXECUTED,
	[PERF_COUNT_HW_CACHE_REFERENCES]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[PERF_COUNT_HW_CACHE_MISSES]		= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[PERF_COUNT_HW_BRANCH_MISSES]		= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[PERF_COUNT_HW_BUS_CYCLES]		= ARMV8_PMUV3_PERFCTR_BUS_CYCLES,
};

const unsigned armv8_pmuv3_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
						[PERF_COUNT_HW_CACHE_OP_MAX]
						[PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,

	[C(BPU)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
};

static const unsigned armv8_a53_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_ACCESS,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_DCACHE_REFILL,
	[C(L1D)][C(OP_PREFETCH)][C(RESULT_MISS)] = ARMV8_A53_PERFCTR_PREFETCH_LINEFILL,

	[C(L1I)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_ICACHE_ACCESS,
	[C(L1I)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_ICACHE_REFILL,

	[C(ITLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_ITLB_REFILL,

	[C(BPU)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
};

static const unsigned armv8_a57_perf_cache_map[PERF_COUNT_HW_CACHE_MAX]
					      [PERF_COUNT_HW_CACHE_OP_MAX]
					      [PERF_COUNT_HW_CACHE_RESULT_MAX] = {
	PERF_CACHE_MAP_ALL_UNSUPPORTED,

	[C(L1D)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_A57_PERFCTR_L1_DCACHE_ACCESS_LD,
	[C(L1D)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_A57_PERFCTR_L1_DCACHE_REFILL_LD,
	[C(L1D)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_A57_PERFCTR_L1_DCACHE_ACCESS_ST,
	[C(L1D)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_A57_PERFCTR_L1_DCACHE_REFILL_ST,

	[C(L1I)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_L1_ICACHE_ACCESS,
	[C(L1I)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_L1_ICACHE_REFILL,

	[C(DTLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_A57_PERFCTR_DTLB_REFILL_LD,
	[C(DTLB)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_A57_PERFCTR_DTLB_REFILL_ST,

	[C(ITLB)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_ITLB_REFILL,

	[C(BPU)][C(OP_READ)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_READ)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_ACCESS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_PRED,
	[C(BPU)][C(OP_WRITE)][C(RESULT_MISS)]	= ARMV8_PMUV3_PERFCTR_PC_BRANCH_MIS_PRED,
};

static ssize_t
armv8pmu_events_sysfs_show(struct device *dev,
			   struct device_attribute *attr, char *page)
{
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr);

	return sprintf(page, "event=0x%03llx\n", pmu_attr->id);
}

#define ARMV8_EVENT_ATTR(name, config) \
	PMU_EVENT_ATTR(name, armv8_event_attr_##name, \
		       ARMV8_PMUV3_PERFCTR_##config, armv8pmu_events_sysfs_show)

ARMV8_EVENT_ATTR(sw_incr, PMNC_SW_INCR);
ARMV8_EVENT_ATTR(l1i_cache_refill, L1_ICACHE_REFILL);
ARMV8_EVENT_ATTR(l1i_tlb_refill, ITLB_REFILL);
ARMV8_EVENT_ATTR(l1d_cache_refill, L1_DCACHE_REFILL);
ARMV8_EVENT_ATTR(l1d_cache, L1_DCACHE_ACCESS);
ARMV8_EVENT_ATTR(l1d_tlb_refill, DTLB_REFILL);
ARMV8_EVENT_ATTR(ld_retired, MEM_READ);
ARMV8_EVENT_ATTR(st_retired, MEM_WRITE);
ARMV8_EVENT_ATTR(inst_retired, INSTR_EXECUTED);
ARMV8_EVENT_ATTR(exc_taken, EXC_TAKEN);
ARMV8_EVENT_ATTR(exc_return, EXC_EXECUTED);
ARMV8_EVENT_ATTR(cid_write_retired, CID_WRITE);
ARMV8_EVENT_ATTR(pc_write_retired, PC_WRITE);
ARMV8_EVENT_ATTR(br_immed_retired, PC_IMM_BRANCH);
ARMV8_EVENT_ATTR(br_return_retired, PC_PROC_RETURN);
ARMV8_EVENT_ATTR(unaligned_ldst_retired, MEM_UNALIGNED_ACCESS);
ARMV8_EVENT_ATTR(br_mis_pred, PC_BRANCH_MIS_PRED);
ARMV8_EVENT_ATTR(cpu_cycles, CLOCK_CYCLES);
ARMV8_EVENT_ATTR(br_pred, PC_BRANCH_PRED);
ARMV8_EVENT_ATTR(mem_access, MEM_ACCESS);
ARMV8_EVENT_ATTR(l1i_cache, L1_ICACHE_ACCESS);
ARMV8_EVENT_ATTR(l1d_cache_wb, L1_DCACHE_WB);
ARMV8_EVENT_ATTR(l2d_cache, L2_CACHE_ACCESS);
ARMV8_EVENT_ATTR(l2d_cache_refill, L2_CACHE_REFILL);
ARMV8_EVENT_ATTR(l2d_cache_wb, L2_CACHE_WB);
ARMV8_EVENT_ATTR(bus_access, BUS_ACCESS);
ARMV8_EVENT_ATTR(memory_error, MEM_ERROR);
ARMV8_EVENT_ATTR(inst_spec, OP_SPEC);
ARMV8_EVENT_ATTR(ttbr_write_retired, TTBR_WRITE);
ARMV8_EVENT_ATTR(bus_cycles, BUS_CYCLES);
ARMV8_EVENT_ATTR(l1d_cache_allocate, L1_DCACHE_ALLOCATE);
ARMV8_EVENT_ATTR(l2d_cache_allocate, L2_CACHE_ALLOCATE);
ARMV8_EVENT_ATTR(br_retired, BR_RETIRED);
ARMV8_EVENT_ATTR(br_mis_pred_retired, BR_MIS_PRED_RETIRED);
ARMV8_EVENT_ATTR(stall_frontend, STALL_FRONTEND);
ARMV8_EVENT_ATTR(stall_backend, STALL_BACKEND);

static struct attribute *armv8_pmuv3_event_attrs[] = {
	&armv8_event_attr_sw_incr.attr.attr,
	&armv8_event_attr_l1i_cache_refill.attr.attr,
	&armv8_event_attr_l1i_tlb_refill.attr.attr,
	&armv8_event_attr_l1d_cache_refill.attr.attr,
	&armv8_event_attr_l1d_cache.attr.attr,
	&armv8_event_attr_l1d_tlb_refill.attr.attr,
	&armv8_event_attr_ld_retired.attr.attr,
	&armv8_event_attr_st_retired.attr.attr,
	&armv8_event_attr_inst_retired.attr.attr,
	&armv8_event_attr_exc_taken.attr.attr,
	&armv8_event_attr_exc_return.attr.attr,
	&armv8_event_attr_cid_write_retired.attr.attr,
	&armv8_event_attr_pc_write_retired.attr.attr,
	&armv8_event_attr_br_immed_retired.attr.attr,
	&armv8_event_attr_br_return_retired.attr.attr,
	&armv8_event_attr_unaligned_ldst_retired.attr.attr,
	&armv8_event_attr_br_mis_pred.attr.attr,
	&armv8_event_attr_cpu_cycles.attr.attr,
	&armv8_event_attr_br_pred.attr.attr,
	&armv8_event_attr_mem_access.attr.attr,
	&armv8_event_attr_l1i_cache.attr.attr,
	&armv8_event_attr_l1d_cache_wb.attr.attr,
	&armv8_event_attr_l2d_cache.attr.attr,
	&armv8_event_attr_l2d_cache_refill.attr.attr,
	&armv8_event_attr_l2d_cache_wb.attr.attr,
	&armv8_event_attr_bus_access.attr.attr,
	&armv8_event_attr_memory_error.attr.attr,
	&armv8_event_attr_inst_spec.attr.attr,
	&armv8_event_attr_ttbr_write_retired.attr.attr,
	&armv8_event_attr_bus_cycles.attr.attr,
	&armv8_event_attr_l1d_cache_allocate.attr.attr,
	&armv8_event_attr_l2d_cache_allocate.attr.attr,
	&armv8_event_attr_br_retired.attr.attr,
	&armv8_event_attr_br_mis_pred_retired.attr.attr,
	&armv8_event_attr_stall_frontend.attr.attr,
	&armv8_event_attr_stall_backend.attr.attr,
	NULL,
};

static umode_t
armv8pmu_event_attr_is_visible(struct kobject *kobj,
			       struct attribute *attr, int unused)
{
	struct device *dev = kobj_to_dev(kobj);
	struct pmu *pmu = dev_get_drvdata(dev);
	struct arm_pmu *cpu_pmu = container_of(pmu, struct arm_pmu, pmu);
	struct perf_pmu_events_attr *pmu_attr;

	pmu_attr = container_of(attr, struct perf_pmu_events_attr, attr.attr);

	if (test_bit(pmu_attr->id, cpu_pmu->pmceid_bitmap))
		return attr->mode;

	return 0;
}

static struct attribute_group armv8_pmuv3_events_attr_group = {
	.name = "events",
	.attrs = armv8_pmuv3_event_attrs,
	.is_visible = armv8pmu_event_attr_is_visible,
};

/*
 * Perf Events' indices
 */
#define	ARMV8_IDX_CYCLE_COUNTER	0
#define	ARMV8_IDX_COUNTER0	1
#define	ARMV8_IDX_COUNTER_LAST(cpu_pmu) \
	(ARMV8_IDX_CYCLE_COUNTER + cpu_pmu->num_events - 1)

#define	ARMV8_MAX_COUNTERS	32
#define	ARMV8_COUNTER_MASK	(ARMV8_MAX_COUNTERS - 1)

/*
 * ARMv8 low level PMU access
 */

/*
 * Perf Event to low level counters mapping
 */
#define	ARMV8_IDX_TO_COUNTER(x)	\
	(((x) - ARMV8_IDX_COUNTER0) & ARMV8_COUNTER_MASK)

/*
 * Per-CPU PMCR: config reg
 */
#define ARMV8_PMCR_E		(1 << 0) /* Enable all counters */
#define ARMV8_PMCR_P		(1 << 1) /* Reset all counters */
#define ARMV8_PMCR_C		(1 << 2) /* Cycle counter reset */
#define ARMV8_PMCR_D		(1 << 3) /* CCNT counts every 64th cpu cycle */
#define ARMV8_PMCR_X		(1 << 4) /* Export to ETM */
#define ARMV8_PMCR_DP		(1 << 5) /* Disable CCNT if non-invasive debug*/
#define	ARMV8_PMCR_N_SHIFT	11	 /* Number of counters supported */
#define	ARMV8_PMCR_N_MASK	0x1f
#define	ARMV8_PMCR_MASK		0x3f	 /* Mask for writable bits */

/*
 * PMOVSR: counters overflow flag status reg
 */
#define	ARMV8_OVSR_MASK		0xffffffff	/* Mask for writable bits */
#define	ARMV8_OVERFLOWED_MASK	ARMV8_OVSR_MASK

/*
 * PMXEVTYPER: Event selection reg
 */
#define	ARMV8_EVTYPE_MASK	0xc80003ff	/* Mask for writable bits */
#define	ARMV8_EVTYPE_EVENT	0x3ff		/* Mask for EVENT bits */

/*
 * Event filters for PMUv3
 */
#define	ARMV8_EXCLUDE_EL1	(1 << 31)
#define	ARMV8_EXCLUDE_EL0	(1 << 30)
#define	ARMV8_INCLUDE_EL2	(1 << 27)

struct arm_pmu_and_idle_nb {
	struct arm_pmu *cpu_pmu;
	struct notifier_block perf_cpu_idle_nb;
};

static inline u32 armv8pmu_pmcr_read(void)
{
	return armv8pmu_pmcr_read_reg();
}

inline void armv8pmu_pmcr_write(u32 val)
{
	val &= ARMV8_PMCR_MASK;
	isb();
	armv8pmu_pmcr_write_reg(val);
}

static inline int armv8pmu_has_overflowed(u32 pmovsr)
{
	return pmovsr & ARMV8_OVERFLOWED_MASK;
}

static inline int armv8pmu_counter_valid(struct arm_pmu *cpu_pmu, int idx)
{
	return idx >= ARMV8_IDX_CYCLE_COUNTER &&
		idx <= ARMV8_IDX_COUNTER_LAST(cpu_pmu);
}

static inline int armv8pmu_counter_has_overflowed(u32 pmnc, int idx)
{
	return pmnc & BIT(ARMV8_IDX_TO_COUNTER(idx));
}

static inline int armv8pmu_select_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	armv8pmu_pmselr_write_reg(counter);
	isb();

	return idx;
}

static inline u32 armv8pmu_read_counter(struct perf_event *event)
{
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;
	u32 value = 0;

	if (!armv8pmu_counter_valid(cpu_pmu, idx))
		pr_err("CPU%u reading wrong counter %d\n",
			smp_processor_id(), idx);
	else if (idx == ARMV8_IDX_CYCLE_COUNTER)
		value = armv8pmu_pmccntr_read_reg();
	else if (armv8pmu_select_counter(idx) == idx)
		value = armv8pmu_pmxevcntr_read_reg();

	return value;
}

static inline void armv8pmu_write_counter(struct perf_event *event, u32 value)
{
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	int idx = hwc->idx;

	if (!armv8pmu_counter_valid(cpu_pmu, idx))
		pr_err("CPU%u writing wrong counter %d\n",
			smp_processor_id(), idx);
	else if (idx == ARMV8_IDX_CYCLE_COUNTER)
		armv8pmu_pmccntr_write_reg(value);
	else if (armv8pmu_select_counter(idx) == idx)
		armv8pmu_pmxevcntr_write_reg(value);
}

inline void armv8pmu_write_evtype(int idx, u32 val)
{
	if (armv8pmu_select_counter(idx) == idx) {
		val &= ARMV8_EVTYPE_MASK;
		armv8pmu_pmxevtyper_write_reg(val);
	}
}

inline int armv8pmu_enable_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	armv8pmu_pmcntenset_write_reg(BIT(counter));
	return idx;
}

inline int armv8pmu_disable_counter(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	armv8pmu_pmcntenclr_write_reg(BIT(counter));
	return idx;
}

inline int armv8pmu_enable_intens(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	armv8pmu_pmintenset_write_reg(BIT(counter));
	return idx;
}

inline int armv8pmu_disable_intens(int idx)
{
	u32 counter = ARMV8_IDX_TO_COUNTER(idx);
	armv8pmu_pmintenclr_write_reg(BIT(counter));
	isb();
	/* Clear the overflow flag in case an interrupt is pending. */
	armv8pmu_pmovsclr_write_reg(BIT(counter));
	isb();

	return idx;
}

inline u32 armv8pmu_getreset_flags(void)
{
	u32 value;

	/* Read */
	value = armv8pmu_pmovsclr_read_reg();

	/* Write to clear flags */
	value &= ARMV8_OVSR_MASK;
	armv8pmu_pmovsclr_write_reg(value);

	return value;
}

static void armv8pmu_enable_event(struct perf_event *event)
{
	unsigned long flags;
	struct hw_perf_event *hwc = &event->hw;
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);
	int idx = hwc->idx;

	/*
	 * Enable counter and interrupt, and set the counter to count
	 * the event that we're interested in.
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/*
	 * Disable counter
	 */
	armv8pmu_disable_counter(idx);

	/*
	 * Set event (if destined for PMNx counters).
	 */
	armv8pmu_write_evtype(idx, hwc->config_base);

	/*
	 * Enable interrupt for this counter
	 */
	armv8pmu_enable_intens(idx);

	/*
	 * Enable counter
	 */
	armv8pmu_enable_counter(idx);

	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void armv8pmu_disable_event(struct perf_event *event)
{
	unsigned long flags;
	struct hw_perf_event *hwc = &event->hw;
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);
	int idx = hwc->idx;

	/*
	 * Disable counter and interrupt
	 */
	raw_spin_lock_irqsave(&events->pmu_lock, flags);

	/*
	 * Disable counter
	 */
	armv8pmu_disable_counter(idx);

	/*
	 * Disable interrupt for this counter
	 */
	armv8pmu_disable_intens(idx);

	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static irqreturn_t armv8pmu_handle_irq(int irq_num, void *dev)
{
	u32 pmovsr;
	struct perf_sample_data data;
	struct arm_pmu *cpu_pmu = (struct arm_pmu *)dev;
	struct pmu_hw_events *cpuc = this_cpu_ptr(cpu_pmu->hw_events);
	struct pt_regs *regs;
	int idx;

	/*
	 * Get and reset the IRQ flags
	 */
	pmovsr = armv8pmu_getreset_flags();

	/*
	 * Did an overflow occur?
	 */
	if (!armv8pmu_has_overflowed(pmovsr))
		return IRQ_NONE;

	/*
	 * Handle the counter(s) overflow(s)
	 */
	regs = get_irq_regs();

	for (idx = 0; idx < cpu_pmu->num_events; ++idx) {
		struct perf_event *event = cpuc->events[idx];
		struct hw_perf_event *hwc;

		/* Ignore if we don't have an event. */
		if (!event)
			continue;

		/*
		 * We have a single interrupt for all counters. Check that
		 * each counter has overflowed before we process it.
		 */
		if (!armv8pmu_counter_has_overflowed(pmovsr, idx))
			continue;

		hwc = &event->hw;
		armpmu_event_update(event);
		perf_sample_data_init(&data, 0, hwc->last_period);
		if (!armpmu_event_set_period(event))
			continue;

		if (perf_event_overflow(event, &data, regs))
			cpu_pmu->disable(event);
	}

	/*
	 * Handle the pending perf events.
	 *
	 * Note: this call *must* be run with interrupts disabled. For
	 * platforms that can have the PMU interrupts raised as an NMI, this
	 * will not work.
	 */
	irq_work_run();

	return IRQ_HANDLED;
}

static void armv8pmu_start(struct arm_pmu *cpu_pmu)
{
	unsigned long flags;
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	raw_spin_lock_irqsave(&events->pmu_lock, flags);
	/* Enable all counters */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() | ARMV8_PMCR_E);
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static void armv8pmu_stop(struct arm_pmu *cpu_pmu)
{
	unsigned long flags;
	struct pmu_hw_events *events = this_cpu_ptr(cpu_pmu->hw_events);

	raw_spin_lock_irqsave(&events->pmu_lock, flags);
	/* Disable all counters */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() & ~ARMV8_PMCR_E);
	raw_spin_unlock_irqrestore(&events->pmu_lock, flags);
}

static int armv8pmu_get_event_idx(struct pmu_hw_events *cpuc,
				  struct perf_event *event)
{
	int idx;
	struct arm_pmu *cpu_pmu = to_arm_pmu(event->pmu);
	struct hw_perf_event *hwc = &event->hw;
	unsigned long evtype = hwc->config_base & ARMV8_EVTYPE_EVENT;

	/* Place the first cycle counter request into the cycle counter. */
	if (evtype == ARMV8_PMUV3_PERFCTR_CLOCK_CYCLES) {
		if (!test_and_set_bit(ARMV8_IDX_CYCLE_COUNTER, cpuc->used_mask))
			return ARMV8_IDX_CYCLE_COUNTER;
	}

	/*
	 * For anything other than a cycle counter, try and use
	 * the events counters
	 */
	for (idx = ARMV8_IDX_COUNTER0; idx < cpu_pmu->num_events; ++idx) {
		if (!test_and_set_bit(idx, cpuc->used_mask))
			return idx;
	}

	/* The counters are all in use. */
	return -EAGAIN;
}

/*
 * Add an event filter to a given event. This will only work for PMUv2 PMUs.
 */
static int armv8pmu_set_event_filter(struct hw_perf_event *event,
				     struct perf_event_attr *attr)
{
	unsigned long config_base = 0;

	if (attr->exclude_user)
		config_base |= ARMV8_EXCLUDE_EL0;
	if (attr->exclude_kernel)
		config_base |= ARMV8_EXCLUDE_EL1;
	if (!attr->exclude_hv)
		config_base |= ARMV8_INCLUDE_EL2;

	/*
	 * Install the filter into config_base as this is used to
	 * construct the event type.
	 */
	event->config_base = config_base;

	return 0;
}

#ifdef CONFIG_PERF_EVENTS_USERMODE
static void armv8pmu_init_usermode(void)
{
	/* Enable access from userspace. */
	armv8pmu_pmuserenr_write_reg(0xF);

}
#else
static inline void armv8pmu_init_usermode(void)
{
	/* Disable access from userspace. */
	armv8pmu_pmuserenr_write_reg(0);

}
#endif


static void armv8pmu_idle_update(struct arm_pmu *cpu_pmu)
{
	struct pmu_hw_events *hw_events;
	struct perf_event *event;
	int idx;

	if (!cpu_pmu)
		return;

	hw_events = this_cpu_ptr(cpu_pmu->hw_events);

	if (!hw_events)
		return;

	for (idx = 0; idx < cpu_pmu->num_events; ++idx) {

		if (!test_bit(idx, hw_events->used_mask))
			continue;

		event = hw_events->events[idx];

		if (!event || !event->attr.exclude_idle ||
				event->state != PERF_EVENT_STATE_ACTIVE)
			continue;

		cpu_pmu->pmu.read(event);
	}
}

static void armv8pmu_reset(void *info)
{
	struct arm_pmu *cpu_pmu = (struct arm_pmu *)info;
	u32 idx, nb_cnt = cpu_pmu->num_events;

	/* The counter and interrupt enable registers are unknown at reset. */
	for (idx = ARMV8_IDX_CYCLE_COUNTER; idx < nb_cnt; ++idx) {
		armv8pmu_disable_counter(idx);
		armv8pmu_disable_intens(idx);
	}

	/* Initialize & Reset PMNC: C and P bits. */
	armv8pmu_pmcr_write(armv8pmu_pmcr_read() | ARMV8_PMCR_P | ARMV8_PMCR_C);

	armv8pmu_init_usermode();
}

static int armv8_pmuv3_map_event(struct perf_event *event)
{
	return armpmu_map_event(event, &armv8_pmuv3_perf_map,
				&armv8_pmuv3_perf_cache_map,
				ARMV8_EVTYPE_EVENT);
}

static int armv8_a53_map_event(struct perf_event *event)
{
	return armpmu_map_event(event, &armv8_a53_perf_map,
				&armv8_a53_perf_cache_map,
				ARMV8_EVTYPE_EVENT);
}

static int armv8_a57_map_event(struct perf_event *event)
{
	return armpmu_map_event(event, &armv8_a57_perf_map,
				&armv8_a57_perf_cache_map,
				ARMV8_EVTYPE_EVENT);
}

static void __armv8pmu_probe_pmu(void *info)
{
	struct arm_pmu *cpu_pmu = info;
	u32 pmceid[2];

	/* Read the nb of CNTx counters supported from PMNC */
	cpu_pmu->num_events = (armv8pmu_pmcr_read() >> ARMV8_PMCR_N_SHIFT)
		& ARMV8_PMCR_N_MASK;

	/* Add the CPU cycles counter */
	cpu_pmu->num_events += 1;

	pmceid[0] = read_sysreg(pmceid0_el0);
	pmceid[1] = read_sysreg(pmceid1_el0);

	cpu_pmu->pmceid_bitmap[0] = pmceid[0];
	cpu_pmu->pmceid_bitmap[0] |= ((unsigned long) pmceid[1]) << 32;
}

static int perf_cpu_idle_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct arm_pmu_and_idle_nb *pmu_nb = container_of(nb,
				struct arm_pmu_and_idle_nb, perf_cpu_idle_nb);

	if (action == IDLE_START)
		armv8pmu_idle_update(pmu_nb->cpu_pmu);

	return NOTIFY_OK;
}

int armv8pmu_probe_num_events(struct arm_pmu *arm_pmu)
{
	int ret;
	struct arm_pmu_and_idle_nb *pmu_idle_nb;

	pmu_idle_nb = devm_kzalloc(&arm_pmu->plat_device->dev,
					sizeof(*pmu_idle_nb), GFP_KERNEL);
	if (!pmu_idle_nb)
		return -ENOMEM;

	pmu_idle_nb->cpu_pmu = arm_pmu;
	pmu_idle_nb->perf_cpu_idle_nb.notifier_call = perf_cpu_idle_notifier;
	idle_notifier_register(&pmu_idle_nb->perf_cpu_idle_nb);

	ret = smp_call_function_any(&arm_pmu->supported_cpus,
				    __armv8pmu_probe_pmu,
				    arm_pmu, 1);
	if (ret)
		idle_notifier_unregister(&pmu_idle_nb->perf_cpu_idle_nb);
	return ret;


}

void armv8_pmu_init(struct arm_pmu *cpu_pmu)
{
	cpu_pmu->handle_irq		= armv8pmu_handle_irq,
	cpu_pmu->enable			= armv8pmu_enable_event,
	cpu_pmu->disable		= armv8pmu_disable_event,
	cpu_pmu->read_counter		= armv8pmu_read_counter,
	cpu_pmu->write_counter		= armv8pmu_write_counter,
	cpu_pmu->get_event_idx		= armv8pmu_get_event_idx,
	cpu_pmu->start			= armv8pmu_start,
	cpu_pmu->stop			= armv8pmu_stop,
	cpu_pmu->reset			= armv8pmu_reset,
	cpu_pmu->max_period		= (1LLU << 32) - 1,
	cpu_pmu->set_event_filter	= armv8pmu_set_event_filter;
}

static int armv8_pmuv3_init(struct arm_pmu *cpu_pmu)
{
	armv8_pmu_init(cpu_pmu);
	cpu_pmu->name			= "armv8_pmuv3";
	cpu_pmu->map_event		= armv8_pmuv3_map_event;
	cpu_pmu->attr_groups[ARMPMU_ATTR_GROUP_EVENTS] =
		&armv8_pmuv3_events_attr_group;
	return armv8pmu_probe_num_events(cpu_pmu);
}

static int armv8_a53_pmu_init(struct arm_pmu *cpu_pmu)
{
	armv8_pmu_init(cpu_pmu);
	cpu_pmu->name			= "armv8_cortex_a53";
	cpu_pmu->map_event		= armv8_a53_map_event;
	return armv8pmu_probe_num_events(cpu_pmu);
}

static int armv8_a57_pmu_init(struct arm_pmu *cpu_pmu)
{
	armv8_pmu_init(cpu_pmu);
	cpu_pmu->name			= "armv8_cortex_a57";
	cpu_pmu->map_event		= armv8_a57_map_event;
	return armv8pmu_probe_num_events(cpu_pmu);
}

static const struct of_device_id armv8_pmu_of_device_ids[] = {
	{.compatible = "arm,armv8-pmuv3",	.data = armv8_pmuv3_init},
	{.compatible = "arm,cortex-a53-pmu",	.data = armv8_a53_pmu_init},
	{.compatible = "arm,cortex-a57-pmu",	.data = armv8_a57_pmu_init},
#ifdef CONFIG_ARCH_MSM8996
	{.compatible = "qcom,kryo-pmuv3", .data = kryo_pmu_init},
#endif
	{},
};

static int armv8_pmu_device_probe(struct platform_device *pdev)
{
	return arm_pmu_device_probe(pdev, armv8_pmu_of_device_ids, NULL);
}

static struct platform_driver armv8_pmu_driver = {
	.driver		= {
		.name	= "armv8-pmu",
		.of_match_table = armv8_pmu_of_device_ids,
		.suppress_bind_attrs = true,
	},
	.probe		= armv8_pmu_device_probe,
};

static int __init register_armv8_pmu_driver(void)
{
	return platform_driver_register(&armv8_pmu_driver);
}
device_initcall(register_armv8_pmu_driver);
