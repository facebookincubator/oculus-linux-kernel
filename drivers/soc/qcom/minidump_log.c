/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/kallsyms.h>
#include <linux/slab.h>
#include <linux/thread_info.h>
#include <soc/qcom/minidump.h>
#include <asm/sections.h>

static void __init register_log_buf(void)
{
	char **log_bufp;
	uint32_t *log_buf_lenp;
	struct md_region md_entry;

	log_bufp = (char **)kallsyms_lookup_name("log_buf");
	log_buf_lenp = (uint32_t *)kallsyms_lookup_name("log_buf_len");
	if (!log_bufp || !log_buf_lenp) {
		pr_err("Unable to find log_buf by kallsyms!\n");
		return;
	}
	/*Register logbuf to minidump, first idx would be from bss section */
	strlcpy(md_entry.name, "KLOGBUF", sizeof(md_entry.name));
	md_entry.virt_addr = (uintptr_t) (*log_bufp);
	md_entry.phys_addr = virt_to_phys(*log_bufp);
	md_entry.size = *log_buf_lenp;
	if (msm_minidump_add_region(&md_entry) < 0)
		pr_err("Failed to add logbuf in Minidump\n");
}

static void __init register_kernel_sections(void)
{
	struct md_region ksec_entry;
	char *data_name = "KDATABSS";
	const size_t static_size = __per_cpu_end - __per_cpu_start;
	void __percpu *base = (void __percpu *)__per_cpu_start;
	unsigned int cpu;

	strlcpy(ksec_entry.name, data_name, sizeof(ksec_entry.name));
	ksec_entry.virt_addr = (uintptr_t)_sdata;
	ksec_entry.phys_addr = virt_to_phys(_sdata);
	ksec_entry.size = roundup((__bss_stop - _sdata), 4);
	if (msm_minidump_add_region(&ksec_entry) < 0)
		pr_err("Failed to add data section in Minidump\n");

	/* Add percpu static sections */
	for_each_possible_cpu(cpu) {
		void *start = per_cpu_ptr(base, cpu);

		memset(&ksec_entry, 0, sizeof(ksec_entry));
		scnprintf(ksec_entry.name, sizeof(ksec_entry.name),
			"KSPERCPU%d", cpu);
		ksec_entry.virt_addr = (uintptr_t)start;
		ksec_entry.phys_addr = per_cpu_ptr_to_phys(start);
		ksec_entry.size = static_size;
		if (msm_minidump_add_region(&ksec_entry) < 0)
			pr_err("Failed to add percpu sections in Minidump\n");
	}
}

void dump_stack_minidump(u64 sp)
{
	struct md_region ksp_entry, ktsk_entry;
	u32 cpu = smp_processor_id();

	if (is_idle_task(current))
		return;

	if (sp < KIMAGE_VADDR || sp > -256UL)
		sp = current_stack_pointer;

	sp &= ~(THREAD_SIZE - 1);
	scnprintf(ksp_entry.name, sizeof(ksp_entry.name), "KSTACK%d", cpu);
	ksp_entry.virt_addr = sp;
	ksp_entry.phys_addr = virt_to_phys((uintptr_t *)sp);
	ksp_entry.size = THREAD_SIZE;
	if (msm_minidump_add_region(&ksp_entry) < 0)
		pr_err("Failed to add stack of cpu %d in Minidump\n", cpu);

	scnprintf(ktsk_entry.name, sizeof(ktsk_entry.name), "KTASK%d", cpu);
	ktsk_entry.virt_addr = (u64)current;
	ktsk_entry.phys_addr = virt_to_phys((uintptr_t *)current);
	ktsk_entry.size = sizeof(struct task_struct);
	if (msm_minidump_add_region(&ktsk_entry) < 0)
		pr_err("Failed to add current task %d in Minidump\n", cpu);
}

static int __init msm_minidump_log_init(void)
{
	register_kernel_sections();
	register_log_buf();
	return 0;
}
late_initcall(msm_minidump_log_init);
