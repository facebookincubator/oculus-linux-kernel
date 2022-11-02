/* SPDX-License-Identifier: GPL-2.0 */

#undef TRACE_SYSTEM
#define TRACE_SYSTEM oculus_instruction_sampler

#if !defined(__OCULUS_INSTRUCTION_SAMPLER_TRACE_H) || defined(TRACE_HEADER_MULTI_READ)
#define __OCULUS_INSTRUCTION_SAMPLER_TRACE_H

#include <linux/types.h>
#include <linux/tracepoint.h>
#include "oculus_instruction_sampler.h"

TRACE_EVENT(cpu_instruction,

	TP_PROTO(int cpu, ktime_t time, const struct sampler_cpu_trace *trace),

	TP_ARGS(cpu, time, trace),

	TP_STRUCT__entry(
		__field(int,		cpu)
		__field(int,		isa)
		__field(ktime_t,	time)
		__field(pid_t,		tgid)
		__field(pid_t,		pid)
		__field(unsigned long,	program_counter)
		__field(unsigned int,	instruction)
		__array(char,		vma_name, SAMPLER_VMA_NAME_LEN)
		__array(char,		task_comm, TASK_COMM_LEN)
	),

	TP_fast_assign(
		__entry->cpu = cpu;
		__entry->isa = trace->isa;
		__entry->time = time;
		__entry->tgid = trace->tgid;
		__entry->pid = trace->pid;
		__entry->program_counter = trace->pc;
		__entry->instruction = trace->instruction;
		memcpy(__entry->vma_name, trace->vma_name, SAMPLER_VMA_NAME_LEN);
		memcpy(__entry->task_comm, trace->task_comm, TASK_COMM_LEN);
	),

	TP_printk("cpu=%d vma=%s#%s+0x%lx time=%lld tid=%d isa=%d instruction=%08x",
		  __entry->cpu, __entry->task_comm, __entry->vma_name, __entry->program_counter,
		  __entry->time, __entry->pid, __entry->isa, __entry->instruction
	)
);

#endif /* __OCULUS_INSTRUCTION_SAMPLER_TRACE_H */

#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH .

#undef TRACE_INCLUDE_FILE
#define TRACE_INCLUDE_FILE oculus_instruction_sampler_trace

#include <trace/define_trace.h>
