/*
 * Record and handle CPU attributes.
 *
 * Copyright (C) 2014 ARM Ltd.
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
#include <asm/arch_timer.h>
#include <asm/cachetype.h>
#include <asm/cpu.h>
#include <asm/cputype.h>
#include <asm/cpufeature.h>

#include <linux/bitops.h>
#include <linux/bug.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/preempt.h>
#include <linux/printk.h>
#include <linux/smp.h>

/*
 * In case the boot CPU is hotpluggable, we record its initial state and
 * current state separately. Certain system registers may contain different
 * values depending on configuration at or after reset.
 */
DEFINE_PER_CPU(struct cpuinfo_arm64, cpu_data);
static struct cpuinfo_arm64 boot_cpu_data;

static char *icache_policy_str[] = {
	[ICACHE_POLICY_RESERVED] = "RESERVED/UNKNOWN",
	[ICACHE_POLICY_AIVIVT] = "AIVIVT",
	[ICACHE_POLICY_VIPT] = "VIPT",
	[ICACHE_POLICY_PIPT] = "PIPT",
};

unsigned long __icache_flags;

static void cpuinfo_detect_icache_policy(struct cpuinfo_arm64 *info)
{
	unsigned int cpu = smp_processor_id();
	u32 l1ip = CTR_L1IP(info->reg_ctr);

	if (l1ip != ICACHE_POLICY_PIPT) {
		/*
		 * VIPT caches are non-aliasing if the VA always equals the PA
		 * in all bit positions that are covered by the index. This is
		 * the case if the size of a way (# of sets * line size) does
		 * not exceed PAGE_SIZE.
		 */
		u32 waysize = icache_get_numsets() * icache_get_linesize();

		if (l1ip != ICACHE_POLICY_VIPT || waysize > PAGE_SIZE)
			set_bit(ICACHEF_ALIASING, &__icache_flags);
	}
	if (l1ip == ICACHE_POLICY_AIVIVT)
		set_bit(ICACHEF_AIVIVT, &__icache_flags);

	pr_debug("Detected %s I-cache on CPU%d\n", icache_policy_str[l1ip], cpu);
}

static void __cpuinfo_store_cpu(struct cpuinfo_arm64 *info)
{
	info->reg_cntfrq = arch_timer_get_cntfrq();
	info->reg_ctr = read_cpuid_cachetype();
	info->reg_dczid = read_cpuid(SYS_DCZID_EL0);
	info->reg_midr = read_cpuid_id();

	info->reg_id_aa64dfr0 = read_cpuid(SYS_ID_AA64DFR0_EL1);
	info->reg_id_aa64dfr1 = read_cpuid(SYS_ID_AA64DFR1_EL1);
	info->reg_id_aa64isar0 = read_cpuid(SYS_ID_AA64ISAR0_EL1);
	info->reg_id_aa64isar1 = read_cpuid(SYS_ID_AA64ISAR1_EL1);
	info->reg_id_aa64mmfr0 = read_cpuid(SYS_ID_AA64MMFR0_EL1);
	info->reg_id_aa64mmfr1 = read_cpuid(SYS_ID_AA64MMFR1_EL1);
	info->reg_id_aa64mmfr2 = read_cpuid(SYS_ID_AA64MMFR2_EL1);
	info->reg_id_aa64pfr0 = read_cpuid(SYS_ID_AA64PFR0_EL1);
	info->reg_id_aa64pfr1 = read_cpuid(SYS_ID_AA64PFR1_EL1);

	info->reg_id_dfr0 = read_cpuid(SYS_ID_DFR0_EL1);
	info->reg_id_isar0 = read_cpuid(SYS_ID_ISAR0_EL1);
	info->reg_id_isar1 = read_cpuid(SYS_ID_ISAR1_EL1);
	info->reg_id_isar2 = read_cpuid(SYS_ID_ISAR2_EL1);
	info->reg_id_isar3 = read_cpuid(SYS_ID_ISAR3_EL1);
	info->reg_id_isar4 = read_cpuid(SYS_ID_ISAR4_EL1);
	info->reg_id_isar5 = read_cpuid(SYS_ID_ISAR5_EL1);
	info->reg_id_mmfr0 = read_cpuid(SYS_ID_MMFR0_EL1);
	info->reg_id_mmfr1 = read_cpuid(SYS_ID_MMFR1_EL1);
	info->reg_id_mmfr2 = read_cpuid(SYS_ID_MMFR2_EL1);
	info->reg_id_mmfr3 = read_cpuid(SYS_ID_MMFR3_EL1);
	info->reg_id_pfr0 = read_cpuid(SYS_ID_PFR0_EL1);
	info->reg_id_pfr1 = read_cpuid(SYS_ID_PFR1_EL1);

	info->reg_mvfr0 = read_cpuid(SYS_MVFR0_EL1);
	info->reg_mvfr1 = read_cpuid(SYS_MVFR1_EL1);
	info->reg_mvfr2 = read_cpuid(SYS_MVFR2_EL1);

	cpuinfo_detect_icache_policy(info);

	check_local_cpu_errata();
}

void cpuinfo_store_cpu(void)
{
	struct cpuinfo_arm64 *info = this_cpu_ptr(&cpu_data);
	__cpuinfo_store_cpu(info);
	update_cpu_features(smp_processor_id(), info, &boot_cpu_data);
}

void __init cpuinfo_store_boot_cpu(void)
{
	struct cpuinfo_arm64 *info = &per_cpu(cpu_data, 0);
	__cpuinfo_store_cpu(info);

	boot_cpu_data = *info;
	init_cpu_features(&boot_cpu_data);
}

u64 __attribute_const__ icache_get_ccsidr(void)
{
	u64 ccsidr;

	WARN_ON(preemptible());

	/* Select L1 I-cache and read its size ID register */
	asm("msr csselr_el1, %1; isb; mrs %0, ccsidr_el1"
	    : "=r"(ccsidr) : "r"(1L));
	return ccsidr;
}
