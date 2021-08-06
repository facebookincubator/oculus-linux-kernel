/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __OCULUS_INSTRUCTION_SAMPLER_H
#define __OCULUS_INSTRUCTION_SAMPLER_H

#include <linux/types.h>

enum sampler_isa {
	ISA_CPU_OFFLINE = 0,
	ISA_CPU_IDLE = 1,
	ISA_AARCH64 = 2,
	ISA_AARCH32 = 3,
	ISA_THUMB = 4,
	ISA_THUMB2 = 5
};

struct sampler_cpu_trace {
	pid_t tgid;
	pid_t pid;
	unsigned long pc;
	u32 instruction;
	enum sampler_isa isa;
};

#endif /* __OCULUS_INSTRUCTION_SAMPLER_H */
