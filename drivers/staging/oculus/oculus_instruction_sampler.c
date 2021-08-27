// SPDX-License-Identifier: GPL-2.0
#include <asm/irq_regs.h>

#include <linux/atomic.h>
#include <linux/compat.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/hrtimer.h>
#include <linux/init.h>
#include <linux/kref.h>
#include <linux/kthread.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/smp.h>

#include <trace/events/sched.h>
#include <uapi/linux/sched/types.h>

#include "oculus_instruction_sampler.h"

#define CREATE_TRACE_POINTS
#include "oculus_instruction_sampler_trace.h"

#define DEFAULT_PERIOD (5 * USEC_PER_MSEC)

/*
 * structure used to latch info about running tasks (and the active
 * instructions) on the system
 */
struct sampler_info {
	struct task_struct *task;
	unsigned long pc;
	enum sampler_isa isa;
};

struct sampler_device {
	struct miscdevice miscdev;
	struct hrtimer timer;
	struct kthread_work work;
	struct kref kref;
	ktime_t last_schedule;
	struct sampler_cpu_trace *traces[2];
	struct sampler_info __percpu *sample_data;
	struct cpumask sample_mask;
	struct mutex device_mutex;
	spinlock_t lock;
	atomic_t enabled;
	atomic_t period;
	atomic_t trace_count;
	int latch_cpu;
};

static struct sampler_device *sampler_dev;

static struct kthread_worker *sampler_kworker;

static const struct file_operations sampler_fops = {
	.owner = THIS_MODULE,
};

#define trace_count_to_index(_cnt) ((_cnt) & 0x1)

static inline bool sampler_is_enabled(struct sampler_device *sampler)
{
	return !!atomic_read_acquire(&sampler->enabled);
}

static void sampler_destroy(struct kref *kref)
{
	struct sampler_device *sampler = container_of(kref, struct sampler_device, kref);
	unsigned int cpu;

	hrtimer_cancel(&sampler->timer);
	kthread_cancel_work_sync(&sampler->work);
	for_each_possible_cpu(cpu) {
		struct sampler_info *info = per_cpu_ptr(sampler->sample_data, cpu);

		if (info->task)
			put_task_struct(info->task);
	}
	free_percpu(sampler->sample_data);
	kfree(sampler->traces[0]);
	kfree(sampler->traces[1]);
	kfree(sampler);
}

static inline void sampler_get(struct sampler_device *sampler)
{
	kref_get(&sampler->kref);
}

static inline void sampler_put(struct sampler_device *sampler)
{
	kref_put(&sampler->kref, sampler_destroy);
}

/* determines which instruction set is in use when the task was interrupted */
static inline enum sampler_isa sampler_divine_isa(struct task_struct *task, struct pt_regs *regs)
{
	if (is_idle_task(task))
		return ISA_CPU_IDLE;
	else if (!is_compat_thread(&task->thread_info))
		return ISA_AARCH64;
	else if (compat_thumb_mode(regs))
		return ISA_THUMB;
	else
		return ISA_AARCH32;
}

static inline void latch_task(struct sampler_info *info, struct task_struct *task)
{
	if (info->task) {
		 /*
		  * must be set to NULL in non-interrupt context to avoid potential deadlock
		  * if put_task_struct results in the task being destroyed, which is a
		  * possible sleeping operation
		  */
		BUG_ON(task);
		put_task_struct(info->task);
		info->task = NULL;
		info->pc = 0;
	}

	if (task) {
		struct pt_regs *regs = task_pt_regs(task);

		info->task = task;
		get_task_struct(task);
		info->pc = instruction_pointer(regs);
		info->isa = sampler_divine_isa(info->task, regs);
	}
}

/* inter-processor interrupt function to sample other processors' program counters */
static void sampler_ipi(void *data)
{
	struct sampler_device *sampler = data;
	int this_cpu = smp_processor_id();
	struct sampler_info *info = per_cpu_ptr(sampler->sample_data, this_cpu);

	latch_task(info, current);

	if (!info->pc) {
		/* if preemption happened in a kernel context, use the IRQ registers to get PC */
		struct pt_regs *regs = get_irq_regs();

		info->pc = instruction_pointer(regs);
		info->isa = sampler_divine_isa(info->task, regs);
	}
}

/* no-op function issued as an IPI to ensure that previous async IPIs are completed */
static void sampler_barrier(void *data)
{
	isb();
}

static inline bool sampler_needs_latch(struct sampler_device *sampler)
{
	unsigned long flags;
	bool work_queued;
	bool already_latched;

	/* verify that data hasn't already been latched waiting for the queued work */
	spin_lock_irqsave(&sampler->lock, flags);
	already_latched = sampler->latch_cpu != -1 || !!cpumask_weight(&sampler->sample_mask);
	spin_unlock_irqrestore(&sampler->lock, flags);

	if (already_latched)
		return false;

	/*
	 * and peek into the work object to verify that it is on the list of queued work.
	 * this would ideally be protected by acquiring the kworker_thread's list lock;
	 * however, this can lead to a deadlock due to lock ordering: kthread_queue_work
	 * calls wake_up_process while holding the worker lock, and various code paths
	 * in the scheduler will lock the task's pi_lock and the target cpu's runqueue
	 * lock (acquired in ttwu_queue, potentially elsewhere). sampler_needs_latch is called
	 * from a sched_switch tracepoint probe, called with the current cpu's runqueue lock
	 * held. try to avoid racy reads, but since this codepath is only called when kworker
	 * is not currently executing (it's called when kworker is the next thread to execute),
	 * the only potential race is with concurrent insertion / deletion of items on the
	 * kworker's list
	 */

	if (!spin_trylock_irqsave(&sampler->work.worker->lock, flags)) {
		smp_rmb();
		return !list_empty(&sampler->work.node);
	}

	work_queued = !list_empty(&sampler->work.node);
	spin_unlock_irqrestore(&sampler->work.worker->lock, flags);

	return work_queued;
}

static void sampler_sched_switch_probe(void *data, bool preempt, struct task_struct *prev, struct task_struct *next)
{
	struct sampler_device *sampler = data;

	if (!sampler || !sampler->work.worker || unlikely(prev == next))
		return;

	/*
	 * if the worker kthread is going to be scheduled, and we haven't already sampled the
	 * running instructions, broadcast an IPI to other CPUs to latch their current tasks,
	 * and latch the previous task on the current CPU that is being preempted to run
	 * the worker. the IPI is sent in this callback to guarantee that it is received before
	 * the worker task begins executing, which could potentially cause the prev task to
	 * migrate to a different CPU (and ultimately be double-sampled if the IPI were issued
	 * in the work function)
	 */
	if (unlikely(next == sampler->work.worker->task) && likely(sampler_needs_latch(sampler))) {
		struct sampler_info *this_info;
		int this_cpu;

		preempt_disable();
		this_cpu = smp_processor_id();
		this_info = per_cpu_ptr(sampler->sample_data, this_cpu);
		cpumask_copy(&sampler->sample_mask, cpu_online_mask);
		cpumask_clear_cpu(this_cpu, &sampler->sample_mask);

		/*
		 * send an asynchronous IPI to all other CPUs. must be asynchronous to
		 * avoid deadlocking the current CPU, since this function is called inside
		 * the core context switch code and runs with interrupts disabled
		 */
		if (likely(cpumask_weight(&sampler->sample_mask) > 0))
			smp_call_function_many(&sampler->sample_mask, sampler_ipi, sampler, false);

		latch_task(this_info, prev);
		sampler->latch_cpu = this_cpu;
		preempt_enable();
	}
}

static void sampler_start_sampler(struct sampler_device *sampler)
{
	int period;
	ktime_t schedule;

	if (!try_module_get(sampler->miscdev.fops->owner))
		return;

	period = atomic_read_acquire(&sampler->period);
	schedule = ns_to_ktime(period * NSEC_PER_USEC);
	sampler_get(sampler);
	atomic_set_release(&sampler->enabled, 1);
	register_trace_sched_switch(sampler_sched_switch_probe, sampler);
	sampler->last_schedule = ktime_add(ktime_get(), schedule);
	hrtimer_start(&sampler->timer, schedule, HRTIMER_MODE_REL);
}

static void sampler_stop_sampler(struct sampler_device *sampler)
{
	atomic_set_release(&sampler->enabled, 0);
	hrtimer_cancel(&sampler->timer);
	kthread_cancel_work_sync(&sampler->work);
	unregister_trace_sched_switch(sampler_sched_switch_probe, sampler);
	sampler_put(sampler);
	module_put(sampler->miscdev.fops->owner);
}

static ssize_t last_trace_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct sampler_device *sampler = container_of(miscdev, struct sampler_device, miscdev);
	struct sampler_cpu_trace *traces;
	ssize_t bytes = 0;
	int cpu, start_count;
	int copy_attempts = 10;

	traces = kzalloc(sizeof(*traces) * num_possible_cpus(), GFP_KERNEL);
	if (!traces)
		return -ENOMEM;

	do {
		struct sampler_cpu_trace *to_copy;

		/*
		 * make sure that the trace data is internally consistent by verifying that
		 * all writes from the worker thread are observable, and that the trace count
		 * doesn't change during the course of the copy
		 */
		smp_rmb();
		start_count = atomic_read_acquire(&sampler->trace_count);
		to_copy = sampler->traces[trace_count_to_index(start_count)];
		memcpy(traces, to_copy, sizeof(*traces) * num_possible_cpus());
	} while (start_count != atomic_read_acquire(&sampler->trace_count) && --copy_attempts > 0);

	if (!copy_attempts) {
		kfree(traces);
		return -EINTR;
	}

	for_each_possible_cpu(cpu) {
		struct sampler_cpu_trace *t = &traces[cpu];
		ssize_t written;

		written = snprintf(buf + bytes, PAGE_SIZE - bytes, "cpu%d: ", cpu);
		if (written < 0)
			break;
		bytes += written;

		switch (t->isa) {
		case ISA_CPU_OFFLINE:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes, "offline\n");
			break;
		case ISA_CPU_IDLE:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes, "idle\n");
			break;
		case ISA_THUMB:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes,
					  "THUMB tid=%d pid=%d pc=%lx inst=%04x\n",
					  t->pid, t->tgid, t->pc, t->instruction);
			break;
		case ISA_THUMB2:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes,
					  "THUMB2 tid=%d pid=%d pc=%lx inst=%08x\n",
					  t->pid, t->tgid, t->pc, t->instruction);
			break;
		case ISA_AARCH32:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes,
					  "A32 tid=%d pid=%d pc=%lx inst=%08x\n",
					  t->pid, t->tgid, t->pc, t->instruction);
			break;
		case ISA_AARCH64:
			written = snprintf(buf + bytes, PAGE_SIZE - bytes,
					  "A64 tid=%d pid=%d pc=%lx inst=%08x\n",
					  t->pid, t->tgid, t->pc, t->instruction);
			break;
		}

		if (written < 0)
			break;
		bytes += written;
	}

	kfree(traces);
	return bytes;
}
static DEVICE_ATTR_RO(last_trace);

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct sampler_device *sampler = container_of(miscdev, struct sampler_device, miscdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", sampler_is_enabled(sampler));
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct sampler_device *sampler = container_of(miscdev, struct sampler_device, miscdev);
	int enable;
	long res;
	int err;

	err = kstrtol(buf, 0, &res);
	if (err < 0)
		return err;

	mutex_lock(&sampler->device_mutex);
	enable = sampler_is_enabled(sampler);

	if (res && !enable)
		sampler_start_sampler(sampler);
	else if (!res && enable)
		sampler_stop_sampler(sampler);
	mutex_unlock(&sampler->device_mutex);

	return count;
}
static DEVICE_ATTR_RW(enabled);

static ssize_t period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct sampler_device *sampler = container_of(miscdev, struct sampler_device, miscdev);
	unsigned long period = (unsigned long)atomic_read_acquire(&sampler->period);

	return snprintf(buf, PAGE_SIZE, "%lu\n", period);
}

static ssize_t period_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct miscdevice *miscdev = dev_get_drvdata(dev);
	struct sampler_device *sampler = container_of(miscdev, struct sampler_device, miscdev);
	int res;
	int err;

	err = kstrtoint(buf, 0, &res);
	if (err < 0)
		return err;

	if (res < 100 || res > 1000000)
		return -EINVAL;

	atomic_set_release(&sampler->period, res);
	return count;
}
static DEVICE_ATTR_RW(period);

static struct attribute *sampler_attrs[] = {
	&dev_attr_enabled.attr,
	&dev_attr_period.attr,
	&dev_attr_last_trace.attr,
	NULL,
};

static const struct attribute_group sampler_group = {
	.attrs = sampler_attrs,
};
__ATTRIBUTE_GROUPS(sampler);

static enum hrtimer_restart sampler_fire(struct hrtimer *timer)
{
	struct sampler_device *sampler = container_of(timer, struct sampler_device, timer);

	kthread_queue_work(sampler_kworker, &sampler->work);
	/*
	 * make sure updates to the work list are observable in case sampler_needs_latch falls back to
	 * the opportunistic (racy) path
	 */
	smp_wmb();
	return HRTIMER_NORESTART;
}

/* returns true if the opcode is the first half of a 32-bit Thumb-2 instruction  */
static inline bool is_thumb_32(u16 first_half)
{
	unsigned int prefix = (first_half >> 11) & 0x1f;

	return prefix == 0x1f || prefix == 0x1e || prefix == 0x1d;
}

/* generates trace data from the latched task sample information */
static bool sampler_create_trace_data(struct sampler_cpu_trace *trace, const struct sampler_info *info)
{
	unsigned long addr = info->pc;
	union {
		u32 a;
		u16 t[2]; /* thumb-2 can be 32-bits wide */
	} i;

	if (!info->task) {
		trace->tgid = 0;
		trace->pid = 0;
		trace->isa = ISA_CPU_OFFLINE;
		return true;
	}

	trace->tgid = task_tgid_nr(info->task);
	trace->pid = task_pid_nr(info->task);
	trace->pc = info->pc;
	trace->isa = info->isa;

	/*
	 * copy the instruction out of the process' virtual address space at the sampled
	 * program counter value. this may fault, so must be called from a preemptible
	 * context
	 */
	switch (info->isa) {
	case ISA_THUMB:
	case ISA_THUMB2:
		/*
		 * ensure address is correctly aligned, adjust the address to reflect the actual
		 * instruction executed, because legacy ARM kept the PC pointing to one instruction
		 * after any reasonable value for it
		 */
		addr -= sizeof(i.t[0]);
		addr &= ~(sizeof(i.t[0]) - 1);
		if (access_process_vm(info->task, addr, &i.t[0], sizeof(i.t[0]), FOLL_FORCE) != sizeof(i.t[0]))
			return false;
		/*
		 * read 32-bit instructions in 2 halves, to ensure that we never dereference an
		 * invalid address if the address happens to be the last two bytes of the page
		 * and the next page is unmapped
		 */
		if (is_thumb_32(i.t[0])) {
			trace->isa = ISA_THUMB2;
			addr += sizeof(i.t[0]);
			if (access_process_vm(info->task, addr, &i.t[1], sizeof(i.t[1]), FOLL_FORCE) != sizeof(i.t[1]))
				return false;
		} else {
			i.t[1] = 0;
		}
		trace->instruction = i.a;
		break;
	case ISA_AARCH32:
		addr -= sizeof(i.a);
		/* fallthrough */
	case ISA_AARCH64:
		addr &= ~(sizeof(i.a) - 1);
		if (access_process_vm(info->task, addr, &i.a,  sizeof(i.a), FOLL_FORCE) != sizeof(i.a))
			return false;
		trace->instruction = i.a;
		break;
	case ISA_CPU_OFFLINE:
	case ISA_CPU_IDLE:
	default:
		break;
	}
	return true;
}

static void sampler_do_work(struct kthread_work *work)
{
	struct sampler_device *sampler = container_of(work, struct sampler_device, work);
	struct cpumask copy_mask;
	unsigned long flags;
	int next_trace;
	int cpu;

	spin_lock_irqsave(&sampler->lock, flags);
	cpumask_copy(&copy_mask, &sampler->sample_mask);
	spin_unlock_irqrestore(&sampler->lock, flags);

	/*
	 * if an async IPI was disaptched to other CPUs, send a synchronous IPI of a no-op
	 * function here so that the previously-dispatched async IPI is guaranteed to
	 * be completed (and all concurrent program counters sampled) before generating
	 * the trace data
	 */
	if (likely(cpumask_weight(&copy_mask) > 0)) {
		preempt_disable();
		smp_call_function_many(&copy_mask, sampler_barrier, NULL, true);
		preempt_enable();
	}

	next_trace = atomic_read_acquire(&sampler->trace_count) + 1;

	for_each_possible_cpu(cpu) {
		struct sampler_info *info = per_cpu_ptr(sampler->sample_data, cpu);
		struct sampler_cpu_trace *trace = &sampler->traces[trace_count_to_index(next_trace)][cpu];

		if (cpumask_test_cpu(cpu, &copy_mask) || cpu == sampler->latch_cpu) {
			sampler_create_trace_data(trace, info);
		} else {
			trace->tgid = 0;
			trace->pid = 0;
			trace->isa = ISA_CPU_OFFLINE;
		}
		trace_cpu_instruction(cpu, sampler->last_schedule, trace);
		latch_task(info, NULL);
	}

	/* ensure all writes to the trace buffer are observable before updating the trace_count */
	smp_wmb();
	atomic_set_release(&sampler->trace_count, next_trace);

	spin_lock_irqsave(&sampler->lock, flags);
	cpumask_clear(&sampler->sample_mask);
	sampler->latch_cpu = -1;
	spin_unlock_irqrestore(&sampler->lock, flags);

	if (sampler_is_enabled(sampler)) {
		int period = atomic_read_acquire(&sampler->period);
		ktime_t target = ktime_add(sampler->last_schedule, ns_to_ktime(period * NSEC_PER_USEC));
		ktime_t now = ktime_get();

		/*
		 * on the off chance that reading back the instruction data took longer
		 * than the sampler period (possible, if the access_process_vm calls triggered
		 * page faults), force the timer into the future
		 */
		if (ktime_before(target, now))
			target = ktime_add(now, ns_to_ktime(period * NSEC_PER_USEC));

		sampler->last_schedule = target;
		target = ktime_sub(target, now);
		hrtimer_start(&sampler->timer, target, HRTIMER_MODE_REL);
	}
}

static struct sampler_device *sampler_create_sampler(const char *name)
{
	struct sampler_device *sampler;
	int err;

	sampler = kzalloc(sizeof(*sampler), GFP_KERNEL);
	if (!sampler) {
		pr_err("instruction_sampler: couldn't allocate sampler structure\n");
		return ERR_PTR(-ENOMEM);
	}

	sampler->traces[0] = kzalloc(sizeof(*sampler->traces[0]) * num_possible_cpus(), GFP_KERNEL);
	if (!sampler->traces[0]) {
		pr_err("instruction_sampler: couldn't allocate trace structures\n");
		err = -ENOMEM;
		goto err_alloc;
	}
	sampler->traces[1] = kzalloc(sizeof(*sampler->traces[1]) * num_possible_cpus(), GFP_KERNEL);
	if (!sampler->traces[0]) {
		pr_err("instruction_sampler: couldn't allocate trace structures\n");
		err = -ENOMEM;
		goto err_alloc_trace1;
	}

	sampler->sample_data = alloc_percpu(struct sampler_info);
	if (!sampler->sample_data) {
		pr_err("instruction_sampler: couldn't allocate sample info structures\n");
		err = -ENOMEM;
		goto err_alloc_trace2;
	}

	kref_init(&sampler->kref);
	mutex_init(&sampler->device_mutex);
	kthread_init_work(&sampler->work, sampler_do_work);
	spin_lock_init(&sampler->lock);
	sampler->miscdev.fops = &sampler_fops;
	sampler->miscdev.minor = MISC_DYNAMIC_MINOR;
	sampler->miscdev.name = name;
	sampler->miscdev.groups = sampler_groups;
	sampler->latch_cpu = -1;
	atomic_set_release(&sampler->period, DEFAULT_PERIOD);
	atomic_set_release(&sampler->trace_count, 0);
	hrtimer_init(&sampler->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	sampler->timer.function = sampler_fire;

	err = misc_register(&sampler->miscdev);
	if (err < 0) {
		pr_err("instruction_sampler: couldn't register miscdev for sampler\n");
		goto err_register;
	}

	return sampler;

err_register:
	free_percpu(sampler->sample_data);
err_alloc_trace2:
	kfree(sampler->traces[1]);
err_alloc_trace1:
	kfree(sampler->traces[0]);
err_alloc:
	kfree(sampler);
	return ERR_PTR(err);
}

static int __init sampler_init(void)
{
	int err;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1,};

	sampler_kworker = kthread_create_worker(0, "instsamplerd");
	if (IS_ERR(sampler_kworker)) {
		pr_err("instruction_sampler: couldn't create sampler kworker\n");
		return PTR_ERR(sampler_kworker);
	}
	sched_setscheduler(sampler_kworker->task, SCHED_FIFO, &param);

	sampler_dev = sampler_create_sampler("instruction_sampler");
	if (IS_ERR(sampler_dev)) {
		pr_err("instruction_sampler: couldn't allocate sampler structure\n");
		err = PTR_ERR(sampler_dev);
		goto err_alloc;
	}

	sampler_get(sampler_dev);
	return 0;

err_alloc:
	kthread_destroy_worker(sampler_kworker);
	return err;
}

static void __exit sampler_exit(void)
{
	misc_deregister(&sampler_dev->miscdev);
	sampler_put(sampler_dev);
	kthread_destroy_worker(sampler_kworker);
	tracepoint_synchronize_unregister();
}

module_init(sampler_init);
module_exit(sampler_exit);
MODULE_DESCRIPTION("Periodic instruction sampler eBPF trace point");
MODULE_LICENSE("GPL v2");
