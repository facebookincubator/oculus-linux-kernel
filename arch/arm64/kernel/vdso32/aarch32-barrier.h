/*
 * Barriers for AArch32 code.
 *
 * Copyright (C) 2016 ARM Ltd.
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
#ifndef __VDSO32_AARCH32_BARRIER_H
#define __VDSO32_AARCH32_BARRIER_H

#include <asm/barrier.h>

#if __LINUX_ARM_ARCH__ >= 8
#define aarch32_smp_mb()	dmb(ish)
#define aarch32_smp_rmb()	dmb(ishld)
#define aarch32_smp_wmb()	dmb(ishst)
#else
#define aarch32_smp_mb()	dmb(ish)
#define aarch32_smp_rmb()	dmb(ish) /* ishld does not exist on ARMv7 */
#define aarch32_smp_wmb()	dmb(ishst)
#endif

#endif	/* __VDSO32_AARCH32_BARRIER_H */
