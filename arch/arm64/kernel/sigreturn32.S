/*
 * sigreturn trampolines for AArch32.
 *
 * Copyright (C) 2005-2011 Nicolas Pitre <nico at fluxnic.net>
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
 *
 *
 * AArch32 sigreturn code
 *
 * For ARM syscalls, the syscall number has to be loaded into r7.
 * We do not support an OABI userspace.
 *
 * For Thumb syscalls, we also pass the syscall number via r7. We therefore
 * need two 16-bit instructions.
 */

#include <asm/unistd.h>

	.globl __aarch32_sigret_code_start
__aarch32_sigret_code_start:

	/*
	 * ARM Code
	 */
	.byte	__NR_compat_sigreturn, 0x70, 0xa0, 0xe3		// mov	r7, #__NR_compat_sigreturn
	.byte	__NR_compat_sigreturn, 0x00, 0x00, 0xef		// svc	#__NR_compat_sigreturn

	/*
	 * Thumb code
	 */
	.byte	__NR_compat_sigreturn, 0x27			// svc	#__NR_compat_sigreturn
	.byte	__NR_compat_sigreturn, 0xdf			// mov	r7, #__NR_compat_sigreturn

	/*
	 * ARM code
	 */
	.byte	__NR_compat_rt_sigreturn, 0x70, 0xa0, 0xe3	// mov	r7, #__NR_compat_rt_sigreturn
	.byte	__NR_compat_rt_sigreturn, 0x00, 0x00, 0xef	// svc	#__NR_compat_rt_sigreturn

	/*
	 * Thumb code
	 */
	.byte	__NR_compat_rt_sigreturn, 0x27			// svc	#__NR_compat_rt_sigreturn
	.byte	__NR_compat_rt_sigreturn, 0xdf			// mov	r7, #__NR_compat_rt_sigreturn

	.globl __aarch32_sigret_code_end
__aarch32_sigret_code_end:
