// SPDX-License-Identifier: GPL-2.0-only
/*
 * arm64 callchain support
 *
 * Copyright (C) 2015 ARM Limited
 */
#include <linux/highmem.h>
#include <linux/perf_event.h>
#include <linux/uaccess.h>

#include <asm/insn.h>
#include <asm/pointer_auth.h>
#include <asm/stacktrace.h>

struct frame_tail {
	struct frame_tail	__user *fp;
	unsigned long		lr;
} __attribute__((packed));

/*
 * Get the return address for a single stackframe and return a pointer to the
 * next frame tail.
 */
static struct frame_tail __user *
user_backtrace(struct frame_tail __user *tail,
	       struct perf_callchain_entry_ctx *entry)
{
	struct frame_tail buftail;
	unsigned long err;
	unsigned long lr;

	/* Also check accessibility of one struct frame_tail beyond */
	if (!access_ok(tail, sizeof(buftail)))
		return NULL;

	pagefault_disable();
	err = __copy_from_user_inatomic(&buftail, tail, sizeof(buftail));
	pagefault_enable();

	if (err)
		return NULL;

	lr = ptrauth_strip_insn_pac(buftail.lr);

	perf_callchain_store(entry, lr);

	/*
	 * Frame pointers should strictly progress back up the stack
	 * (towards higher addresses).
	 */
	if (tail >= buftail.fp)
		return NULL;

	return buftail.fp;
}

#ifdef CONFIG_COMPAT
/*
 * The registers we're interested in are at the end of the variable
 * length saved register structure. The fp points at the end of this
 * structure so the address of this struct is:
 * (struct compat_frame_tail *)(xxx->fp)-1
 *
 * This code has been adapted from the ARM OProfile support.
 */
struct compat_frame_tail {
	compat_uptr_t	fp; /* a (struct compat_frame_tail *) in compat mode */
	u32		sp;
	u32		lr;
} __attribute__((packed));

static struct compat_frame_tail __user *
compat_user_backtrace(struct compat_frame_tail __user *tail,
		      struct perf_callchain_entry_ctx *entry)
{
	struct compat_frame_tail buftail;
	unsigned long err;

	/* Also check accessibility of one struct frame_tail beyond */
	if (!access_ok(tail, sizeof(buftail)))
		return NULL;

	pagefault_disable();
	err = __copy_from_user_inatomic(&buftail, tail, sizeof(buftail));
	pagefault_enable();

	if (err)
		return NULL;

	perf_callchain_store(entry, buftail.lr);

	/*
	 * Frame pointers should strictly progress back up the stack
	 * (towards higher addresses).
	 */
	if (tail + 1 >= (struct compat_frame_tail __user *)
			compat_ptr(buftail.fp))
		return NULL;

	return (struct compat_frame_tail __user *)compat_ptr(buftail.fp) - 1;
}
#endif /* CONFIG_COMPAT */

void perf_callchain_user(struct perf_callchain_entry_ctx *entry,
			 struct pt_regs *regs)
{
	struct perf_guest_info_callbacks *guest_cbs = perf_get_guest_cbs();

	if (guest_cbs && guest_cbs->is_in_guest()) {
		/* We don't support guest os callchain now */
		return;
	}

	perf_callchain_store(entry, regs->pc);

	if (!compat_user_mode(regs)) {
		/* AARCH64 mode */
		struct frame_tail __user *tail;

		tail = (struct frame_tail __user *)regs->regs[29];

		while (entry->nr < entry->max_stack &&
		       tail && !((unsigned long)tail & 0xf))
			tail = user_backtrace(tail, entry);
	} else {
#ifdef CONFIG_COMPAT
		/* AARCH32 compat mode */
		struct compat_frame_tail __user *tail;

		tail = (struct compat_frame_tail __user *)regs->compat_fp - 1;

		while ((entry->nr < entry->max_stack) &&
			tail && !((unsigned long)tail & 0x3))
			tail = compat_user_backtrace(tail, entry);
#endif
	}
}

/*
 * Gets called by walk_stackframe() for every stackframe. This will be called
 * whist unwinding the stackframe and is like a subroutine return so we use
 * the PC.
 */
static bool callchain_trace(void *data, unsigned long pc)
{
	struct perf_callchain_entry_ctx *entry = data;
	perf_callchain_store(entry, pc);
	return true;
}

void perf_callchain_kernel(struct perf_callchain_entry_ctx *entry,
			   struct pt_regs *regs)
{
	struct perf_guest_info_callbacks *guest_cbs = perf_get_guest_cbs();
	struct stackframe frame;

	if (guest_cbs && guest_cbs->is_in_guest()) {
		/* We don't support guest os callchain now */
		return;
	}

	start_backtrace(&frame, regs->regs[29], regs->pc);
	walk_stackframe(current, &frame, callchain_trace, entry);
}

unsigned long perf_instruction_pointer(struct pt_regs *regs)
{
	struct perf_guest_info_callbacks *guest_cbs = perf_get_guest_cbs();

	if (guest_cbs && guest_cbs->is_in_guest())
		return guest_cbs->get_guest_ip();

	return instruction_pointer(regs);
}

unsigned long perf_misc_flags(struct pt_regs *regs)
{
	struct perf_guest_info_callbacks *guest_cbs = perf_get_guest_cbs();
	int misc = 0;

	if (guest_cbs && guest_cbs->is_in_guest()) {
		if (guest_cbs->is_user_mode())
			misc |= PERF_RECORD_MISC_GUEST_USER;
		else
			misc |= PERF_RECORD_MISC_GUEST_KERNEL;
	} else {
		if (user_mode(regs))
			misc |= PERF_RECORD_MISC_USER;
		else
			misc |= PERF_RECORD_MISC_KERNEL;
	}

	return misc;
}

size_t perf_instruction_data(u8 *buf, size_t sz_buf, struct pt_regs *regs)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long addr, offset;
	long nr_pages;
	size_t copied = 0;
	int i, locked;

	if (perf_guest_cbs && perf_guest_cbs->is_in_guest())
		goto out;

	if (sz_buf < AARCH64_INSN_SIZE + sizeof(regs->pstate))
		goto out;

	addr = instruction_pointer(regs);

	if (compat_user_mode(regs)) {
		/*
		 * Thumb instructions will be either 16 or 32 bits, aligned to 16b addresses. 32-bit
		 * instructions are structured as two 16-bit half-words, with a sentinel bit pattern
		 * in the first half-word identifying the length of the instruction. so, read 4 bytes
		 * regardless of thumb vs arm mode, since the thumb decoder will need all 32 bits to
		 * determine the length of the instruction
		 */
		addr -= AARCH64_INSN_SIZE;
		addr &= (compat_thumb_mode(regs) ? ~1 : ~3);
	} else {
		addr &= ~3;
	}

	if (addr < PAGE_SIZE)
		goto out;

	if (!user_mode(regs)) {
		if (virt_addr_valid(addr) || is_vmalloc_addr((void *)addr) || (__module_address(addr) != NULL)) {
			memcpy(buf, (void *)addr, AARCH64_INSN_SIZE);
			copied = AARCH64_INSN_SIZE;
		}
		goto out;
	}

	/* executed in hrtimer irq context, so sleeping isn't allowed */
	for (i = 0, locked = 0; i < AARCH64_INSN_SIZE && !locked; i++) {
		locked = down_read_trylock(&mm->mmap_lock);
		if (!locked)
			cpu_relax();
	}

	if (!locked)
		goto out;

	offset = offset_in_page(addr);
	addr &= PAGE_MASK;

	vma = find_vma(mm, addr);
	if (!vma || vma->vm_end < addr + offset + AARCH64_INSN_SIZE)
		goto out_locked;

	/* 32-bit thumb instructions create an edge case where instructions may span 2 pages. */
	nr_pages = 1 + !!(PAGE_SIZE - offset < AARCH64_INSN_SIZE);

	for (i = 0; i < nr_pages; i++, offset = 0, addr += PAGE_SIZE) {
		struct page *page = follow_page(vma, addr, FOLL_FORCE | FOLL_NOWAIT);
		size_t bytes = min((unsigned long)AARCH64_INSN_SIZE, PAGE_SIZE - offset) - copied;
		void *maddr;

		if (IS_ERR_OR_NULL(page))
			break;
		get_page(page);
		maddr = kmap_atomic(page);
		WARN_ON_ONCE(!maddr);
		if (maddr) {
			copy_from_user_page(NULL, page, addr + offset, buf + copied, maddr + offset, bytes);
			copied += bytes;
			kunmap_atomic(maddr);
		}
		put_page(page);
		/* offset and addr updated in loop control statement */
	}

 out_locked:
	up_read(&mm->mmap_lock);
 out:
	if (copied == AARCH64_INSN_SIZE) {
		/*
		 * user-space instruction disassembly needs to know the processor state to
		 * determine whether compat-mode applications were executing in thumb mode
		 */
		memcpy(buf + AARCH64_INSN_SIZE, &regs->pstate, sizeof(regs->pstate));
		copied += sizeof(regs->pstate);
	}
	return copied;
}

