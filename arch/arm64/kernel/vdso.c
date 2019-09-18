/*
 * VDSO implementation for AArch64 and vector page setup for AArch32.
 * Additional userspace pages setup for AArch64 and AArch32.
 * - AArch64: vDSO pages setup, vDSO data page update.
 * - AArch32: sigreturn and kuser helpers pages setup.
 *
 * Copyright (C) 2012 ARM Limited
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
 *
 * Author: Will Deacon <will.deacon@arm.com>
 */

#include <linux/kernel.h>
#include <linux/clocksource.h>
#include <linux/elf.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/timekeeper_internal.h>
#include <linux/vmalloc.h>

#include <asm/cacheflush.h>
#include <asm/signal32.h>
#include <asm/vdso.h>
#include <asm/vdso_datapage.h>


#ifndef __ro_after_init
#define __ro_after_init __attribute__((__section__(".data..ro_after_init")))
#endif

#ifndef PHYS_PFN
#define PHYS_PFN(x)	((unsigned long)((x) >> PAGE_SHIFT))
#endif

struct vdso_mappings {
	unsigned long num_code_pages;
	struct vm_special_mapping data_mapping, code_mapping;
};

/*
 * The vDSO data page.
 */
static union {
	struct vdso_data	data;
	u8			page[PAGE_SIZE];
} vdso_data_store __page_aligned_data;
struct vdso_data *vdso_data = &vdso_data_store.data;

static int __init vdso_mappings_init(const char *name,
				      const char *code_start,
				      const char *code_end,
				      struct vdso_mappings *mappings)
{
	unsigned long i, num_code_pages;
	struct page **pages;

	if (memcmp(code_start, "\177ELF", 4)) {
		pr_err("%s is not a valid ELF object!\n", name);
		return -EINVAL;
	}

	num_code_pages = (code_end - code_start) >> PAGE_SHIFT;
	pr_info("%s: %ld pages (%ld code @ %p, %ld data @ %p)\n",
		name, num_code_pages + 1, num_code_pages, code_start, 1L,
		vdso_data);

	/*
	 * Allocate space for storing pointers to the vDSO code pages + the
	 * data page. The pointers must have the same lifetime as the mappings,
	 * which are static, so there is no need to keep track of the pointer
	 * array to free it.
	 */
	pages = kmalloc_array(num_code_pages + 1, sizeof(struct page *),
			      GFP_KERNEL);
	if (pages == NULL)
		return -ENOMEM;

	/* Grab the vDSO data page */
	pages[0] = pfn_to_page(PHYS_PFN(__pa(vdso_data)));

	/* Grab the vDSO code pages */
	for (i = 0; i < num_code_pages; i++)
		pages[i + 1] = pfn_to_page(PHYS_PFN(__pa(code_start)) + i);

	/* Populate the special mapping structures */
	mappings->data_mapping = (struct vm_special_mapping) {
		.name	= "[vvar]",
		.pages	= &pages[0],
	};

	mappings->code_mapping = (struct vm_special_mapping) {
		.name	= "[vdso]",
		.pages	= &pages[1],
	};

	mappings->num_code_pages = num_code_pages;
	return 0;
}

static int vdso_setup(struct mm_struct *mm,
		      const struct vdso_mappings *mappings)
{
	unsigned long vdso_base, vdso_text_len, vdso_mapping_len;
	void *ret;

	vdso_text_len = mappings->num_code_pages << PAGE_SHIFT;
	/* Be sure to map the data page */
	vdso_mapping_len = vdso_text_len + PAGE_SIZE;

	vdso_base = get_unmapped_area(NULL, 0, vdso_mapping_len, 0, 0);
	if (IS_ERR_VALUE(vdso_base)) {
		ret = ERR_PTR(vdso_base);
		goto out;
	}

	ret = _install_special_mapping(mm, vdso_base, PAGE_SIZE,
				       VM_READ|VM_MAYREAD,
				       &mappings->data_mapping);
	if (IS_ERR(ret))
		goto out;

	vdso_base += PAGE_SIZE;
	ret = _install_special_mapping(mm, vdso_base, vdso_text_len,
				       VM_READ|VM_EXEC|
				       VM_MAYREAD|VM_MAYWRITE|VM_MAYEXEC,
				       &mappings->code_mapping);
	if (IS_ERR(ret))
		goto out;

	mm->context.vdso = (void *)vdso_base;

out:
	return PTR_ERR_OR_ZERO(ret);
}

#ifdef CONFIG_COMPAT
#ifdef CONFIG_VDSO32

static struct vdso_mappings vdso32_mappings __ro_after_init;

static int __init vdso32_init(void)
{
	extern char vdso32_start, vdso32_end;

	return vdso_mappings_init("vdso32", &vdso32_start, &vdso32_end,
				  &vdso32_mappings);
}
arch_initcall(vdso32_init);

#else /* CONFIG_VDSO32 */

/* sigreturn trampolines page */
static struct page *sigreturn_page __ro_after_init;
static const struct vm_special_mapping sigreturn_spec = {
	.name = "[sigreturn]",
	.pages = &sigreturn_page,
};

static int __init aarch32_sigreturn_init(void)
{
	extern char __aarch32_sigret_code_start, __aarch32_sigret_code_end;

	size_t sigret_sz =
		&__aarch32_sigret_code_end - &__aarch32_sigret_code_start;
	struct page *page;
	unsigned long page_addr;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page)
		return -ENOMEM;
	page_addr = (unsigned long)page_address(page);

	memcpy((void *)page_addr, &__aarch32_sigret_code_start, sigret_sz);

	flush_icache_range(page_addr, page_addr + PAGE_SIZE);

	sigreturn_page = page;
	return 0;
}
arch_initcall(aarch32_sigreturn_init);

static int sigreturn_setup(struct mm_struct *mm)
{
	unsigned long addr;
	void *ret;

	addr = get_unmapped_area(NULL, 0, PAGE_SIZE, 0, 0);
	if (IS_ERR_VALUE(addr)) {
		ret = ERR_PTR(addr);
		goto out;
	}

	ret = _install_special_mapping(mm, addr, PAGE_SIZE,
				       VM_READ|VM_EXEC|
				       VM_MAYREAD|VM_MAYWRITE|VM_MAYEXEC,
				       &sigreturn_spec);
	if (IS_ERR(ret))
		goto out;

	mm->context.vdso = (void *)addr;

out:
	return PTR_ERR_OR_ZERO(ret);
}

#endif /* CONFIG_VDSO32 */

#ifdef CONFIG_KUSER_HELPERS

/* kuser helpers page */
static struct page *kuser_helpers_page __ro_after_init;
static const struct vm_special_mapping kuser_helpers_spec = {
	.name	= "[kuserhelpers]",
	.pages	= &kuser_helpers_page,
};

static int __init aarch32_kuser_helpers_init(void)
{
	extern char __kuser_helper_start, __kuser_helper_end;

	size_t kuser_sz = &__kuser_helper_end - &__kuser_helper_start;
	struct page *page;
	unsigned long page_addr;

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page)
		return -ENOMEM;
	page_addr = (unsigned long)page_address(page);

	memcpy((void *)(page_addr + 0x1000 - kuser_sz), &__kuser_helper_start,
	       kuser_sz);

	flush_icache_range(page_addr, page_addr + PAGE_SIZE);

	kuser_helpers_page = page;
	return 0;
}
arch_initcall(aarch32_kuser_helpers_init);

static int kuser_helpers_setup(struct mm_struct *mm)
{
	void *ret;

	/* Map the kuser helpers at the ABI-defined high address */
	ret = _install_special_mapping(
			mm, AARCH32_KUSER_HELPERS_BASE, PAGE_SIZE,
			VM_READ|VM_EXEC|VM_MAYREAD|VM_MAYEXEC,
			&kuser_helpers_spec);
	return PTR_ERR_OR_ZERO(ret);
}

#endif /* CONFIG_KUSER_HELPERS */

int aarch32_setup_additional_pages(struct linux_binprm *bprm, int uses_interp)
{
	struct mm_struct *mm = current->mm;
	int ret;

	down_write(&mm->mmap_sem);

#ifdef CONFIG_VDSO32
	ret = vdso_setup(mm, &vdso32_mappings);
#else
	ret = sigreturn_setup(mm);
#endif
	if (ret)
		goto out;

#ifdef CONFIG_KUSER_HELPERS
	ret = kuser_helpers_setup(mm);
#endif

out:
	up_write(&mm->mmap_sem);
	return ret;
}

#endif /* CONFIG_COMPAT */

static struct vdso_mappings vdso_mappings __ro_after_init;

static int __init vdso_init(void)
{
	extern char vdso_start, vdso_end;

	return vdso_mappings_init("vdso", &vdso_start, &vdso_end,
				   &vdso_mappings);
}
arch_initcall(vdso_init);

int arch_setup_additional_pages(struct linux_binprm *bprm,
				int uses_interp)
{
	struct mm_struct *mm = current->mm;
	int ret;

	ret = vdso_setup(mm, &vdso_mappings);

	return ret;
}

/*
 * Update the vDSO data page to keep in sync with kernel timekeeping.
 */
void update_vsyscall(struct timekeeper *tk)
{
	u32 use_syscall = strcmp(tk->tkr_mono.clock->name, "arch_sys_counter");

	++vdso_data->tb_seq_count;
	smp_wmb();

	vdso_data->use_syscall			= use_syscall;
	vdso_data->xtime_coarse_sec		= tk->xtime_sec;
	vdso_data->xtime_coarse_nsec		= tk->tkr_mono.xtime_nsec >>
							tk->tkr_mono.shift;
	vdso_data->wtm_clock_sec		= tk->wall_to_monotonic.tv_sec;
	vdso_data->wtm_clock_nsec		= tk->wall_to_monotonic.tv_nsec;

	if (!use_syscall) {
		vdso_data->cs_cycle_last	= tk->tkr_mono.cycle_last;
		vdso_data->xtime_clock_sec	= tk->xtime_sec;
		vdso_data->xtime_clock_nsec	= tk->tkr_mono.xtime_nsec;
		vdso_data->cs_mult		= tk->tkr_mono.mult;
		vdso_data->cs_shift		= tk->tkr_mono.shift;
	}

	smp_wmb();
	++vdso_data->tb_seq_count;
}

void update_vsyscall_tz(void)
{
	vdso_data->tz_minuteswest	= sys_tz.tz_minuteswest;
	vdso_data->tz_dsttime		= sys_tz.tz_dsttime;
}
