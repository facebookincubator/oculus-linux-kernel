/*
 * Stack tracing support
 *
 * Copyright (C) 2012 ARM Ltd.
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
#include <linux/compat.h>
#include <linux/kasan.h>
#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/ftrace.h>
#include <linux/sched.h>
#include <linux/stacktrace.h>
#include <linux/uaccess.h>

#include <asm/irq.h>
#include <asm/stack_pointer.h>
#include <asm/stacktrace.h>

/*
 * AArch64 PCS assigns the frame pointer to x29.
 *
 * A simple function prologue looks like this:
 * 	sub	sp, sp, #0x10
 *   	stp	x29, x30, [sp]
 *	mov	x29, sp
 *
 * A simple function epilogue looks like this:
 *	mov	sp, x29
 *	ldp	x29, x30, [sp]
 *	add	sp, sp, #0x10
 */
int notrace unwind_frame(struct task_struct *tsk, struct stackframe *frame)
{
	unsigned long high, low;
	unsigned long fp = frame->fp;
	unsigned long irq_stack_ptr;

	/*
	 * Switching between stacks is valid when tracing current and in
	 * non-preemptible context.
	 */
	if (tsk == current && !preemptible())
		irq_stack_ptr = IRQ_STACK_PTR(smp_processor_id());
	else
		irq_stack_ptr = 0;

	low  = frame->sp;
	/* irq stacks are not THREAD_SIZE aligned */
	if (on_irq_stack(frame->sp, raw_smp_processor_id()))
		high = irq_stack_ptr;
	else
		high = ALIGN(low, THREAD_SIZE) - 0x20;

	if (fp < low || fp > high || fp & 0xf)
		return -EINVAL;

	kasan_disable_current();

	frame->sp = fp + 0x10;
	frame->fp = *(unsigned long *)(fp);
	frame->pc = *(unsigned long *)(fp + 8);

	kasan_enable_current();

#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	if (tsk && tsk->ret_stack &&
			(frame->pc == (unsigned long)return_to_handler)) {
		/*
		 * This is a case where function graph tracer has
		 * modified a return address (LR) in a stack frame
		 * to hook a function return.
		 * So replace it to an original value.
		 */
		frame->pc = tsk->ret_stack[frame->graph--].ret;
	}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

	/*
	 * Check whether we are going to walk through from interrupt stack
	 * to task stack.
	 * If we reach the end of the stack - and its an interrupt stack,
	 * unpack the dummy frame to find the original elr.
	 *
	 * Check the frame->fp we read from the bottom of the irq_stack,
	 * and the original task stack pointer are both in current->stack.
	 */
	if (frame->sp == irq_stack_ptr) {
		struct pt_regs *irq_args;
		unsigned long orig_sp = IRQ_STACK_TO_TASK_STACK(irq_stack_ptr);

		if (object_is_on_stack((void *)orig_sp) &&
		   object_is_on_stack((void *)frame->fp)) {
			frame->sp = orig_sp;

			/* orig_sp is the saved pt_regs, find the elr */
			irq_args = (struct pt_regs *)orig_sp;
			frame->pc = irq_args->pc;
		} else {
			/*
			 * This frame has a non-standard format, and we
			 * didn't fix it, because the data looked wrong.
			 * Refuse to output this frame.
			 */
			return -EINVAL;
		}
	}

	return 0;
}

void notrace walk_stackframe(struct task_struct *tsk, struct stackframe *frame,
		     int (*fn)(struct stackframe *, void *), void *data)
{
	while (1) {
		int ret;

		if (fn(frame, data))
			break;
		ret = unwind_frame(tsk, frame);
		if (ret < 0)
			break;
	}
}

#ifdef CONFIG_STACKTRACE
struct stack_trace_data {
	struct stack_trace *trace;
	unsigned int no_sched_functions;
	unsigned int skip;
};

static int save_trace(struct stackframe *frame, void *d)
{
	struct stack_trace_data *data = d;
	struct stack_trace *trace = data->trace;
	unsigned long addr = frame->pc;

	if (data->no_sched_functions && in_sched_functions(addr))
		return 0;
	if (data->skip) {
		data->skip--;
		return 0;
	}

	trace->entries[trace->nr_entries++] = addr;

	return trace->nr_entries >= trace->max_entries;
}

static noinline void __save_stack_trace(struct task_struct *tsk,
	struct stack_trace *trace, unsigned int nosched)
{
	struct stack_trace_data data;
	struct stackframe frame;

	if (!try_get_task_stack(tsk))
		return;

	data.trace = trace;
	data.skip = trace->skip;
	data.no_sched_functions = nosched;

	if (tsk != current) {
		frame.fp = thread_saved_fp(tsk);
		frame.sp = thread_saved_sp(tsk);
		frame.pc = thread_saved_pc(tsk);
	} else {
		/* We don't want this function nor the caller */
		data.skip += 2;
		frame.fp = (unsigned long)__builtin_frame_address(0);
		frame.sp = current_stack_pointer;
		frame.pc = (unsigned long)__save_stack_trace;
	}
#ifdef CONFIG_FUNCTION_GRAPH_TRACER
	frame.graph = tsk->curr_ret_stack;
#endif

	walk_stackframe(tsk, &frame, save_trace, &data);
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = ULONG_MAX;

	put_task_stack(tsk);
}
EXPORT_SYMBOL(save_stack_trace_tsk);

void save_stack_trace_tsk(struct task_struct *tsk, struct stack_trace *trace)
{
	__save_stack_trace(tsk, trace, 1);
}

void save_stack_trace(struct stack_trace *trace)
{
	__save_stack_trace(current, trace, 0);
}
EXPORT_SYMBOL_GPL(save_stack_trace);

static void add_trace_entry(struct stack_trace *trace, unsigned long pc)
{
	if (trace->nr_entries < trace->max_entries)
		trace->entries[trace->nr_entries++] = pc;
}

static const void __user *stack_walk_next(
	struct stack_trace *trace, const void __user *fp, int compat)
{
	/* Part of the stack containing saved FP and LR registers. */
	unsigned long frame_tail[2];
	const size_t tail_size = compat ? 8 : sizeof(frame_tail);

	/*
	 * PCS specifies the minimum SP alignment to be 4 and 16 bytes
	 * for AArch32 and AArch64, respectively.
	 */
	const size_t sp_mask = compat ? 0x3 : 0xf;

	const void __user *prev_fp;
	int ret;

	BUILD_BUG_ON(tail_size > sizeof(frame_tail));
	if (!access_ok(VERIFY_READ, fp, tail_size))
		return NULL;

	pagefault_disable();
	ret = __copy_from_user_inatomic(&frame_tail[0], fp, tail_size);
	pagefault_enable();

	if (ret)
		return NULL;

	if (compat) {
		u32 *tail = (u32 *)&frame_tail[0];

		prev_fp = (const void __user *)(unsigned long)tail[0];
		add_trace_entry(trace, tail[1]);
	} else {
		prev_fp = (const void __user *)frame_tail[0];
		add_trace_entry(trace, frame_tail[1]);
	}

	/*
	 * Since the stack grows downwards, previous frame pointer is expected
	 * to be at a higher memory address.
	 */
	if (prev_fp > fp && !((unsigned long)prev_fp & sp_mask))
		return prev_fp;
	return NULL;
}

void save_stack_trace_user(struct stack_trace *trace)
{
	const struct pt_regs *regs = task_pt_regs(current);
	const int compat = compat_user_mode(regs);
	const void __user *fp;

	add_trace_entry(trace, regs->pc);

	/*
	 * Leaf functions may not have separate stack frames, so always
	 * include the value of the link register to make sure we don't
	 * skip such functions.
	 */
	if (compat) {
		add_trace_entry(trace, regs->compat_lr);
		fp = (const void __user *)regs->compat_fp;
	} else {
		add_trace_entry(trace, regs->regs[30]);
		fp = (const void __user *)regs->regs[29];
	}

	while (fp && trace->nr_entries < trace->max_entries)
		fp = stack_walk_next(trace, fp, compat);
}

#endif
