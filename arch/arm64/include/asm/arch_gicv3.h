/*
 * arch/arm64/include/asm/arch_gicv3.h
 *
 * Copyright (C) 2015 ARM Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
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
#ifndef __ASM_ARCH_GICV3_H
#define __ASM_ARCH_GICV3_H

#include <asm/sysreg.h>

#define ICC_EOIR1_EL1			sys_reg(3, 0, 12, 12, 1)
#define ICC_HPPIR1_EL1			sys_reg(3, 0, 12, 12, 2)
#define ICC_DIR_EL1			sys_reg(3, 0, 12, 11, 1)
#define ICC_IAR1_EL1			sys_reg(3, 0, 12, 12, 0)
#define ICC_SGI1R_EL1			sys_reg(3, 0, 12, 11, 5)
#define ICC_PMR_EL1			sys_reg(3, 0, 4, 6, 0)
#define ICC_CTLR_EL1			sys_reg(3, 0, 12, 12, 4)
#define ICC_SRE_EL1			sys_reg(3, 0, 12, 12, 5)
#define ICC_GRPEN1_EL1			sys_reg(3, 0, 12, 12, 7)
#define ICC_BPR1_EL1			sys_reg(3, 0, 12, 12, 3)

#define ICC_SRE_EL2			sys_reg(3, 4, 12, 9, 5)

/*
 * System register definitions
 */
#define ICH_VSEIR_EL2			sys_reg(3, 4, 12, 9, 4)
#define ICH_HCR_EL2			sys_reg(3, 4, 12, 11, 0)
#define ICH_VTR_EL2			sys_reg(3, 4, 12, 11, 1)
#define ICH_MISR_EL2			sys_reg(3, 4, 12, 11, 2)
#define ICH_EISR_EL2			sys_reg(3, 4, 12, 11, 3)
#define ICH_ELSR_EL2			sys_reg(3, 4, 12, 11, 5)
#define ICH_VMCR_EL2			sys_reg(3, 4, 12, 11, 7)

#define __LR0_EL2(x)			sys_reg(3, 4, 12, 12, x)
#define __LR8_EL2(x)			sys_reg(3, 4, 12, 13, x)

#define ICH_LR0_EL2			__LR0_EL2(0)
#define ICH_LR1_EL2			__LR0_EL2(1)
#define ICH_LR2_EL2			__LR0_EL2(2)
#define ICH_LR3_EL2			__LR0_EL2(3)
#define ICH_LR4_EL2			__LR0_EL2(4)
#define ICH_LR5_EL2			__LR0_EL2(5)
#define ICH_LR6_EL2			__LR0_EL2(6)
#define ICH_LR7_EL2			__LR0_EL2(7)
#define ICH_LR8_EL2			__LR8_EL2(0)
#define ICH_LR9_EL2			__LR8_EL2(1)
#define ICH_LR10_EL2			__LR8_EL2(2)
#define ICH_LR11_EL2			__LR8_EL2(3)
#define ICH_LR12_EL2			__LR8_EL2(4)
#define ICH_LR13_EL2			__LR8_EL2(5)
#define ICH_LR14_EL2			__LR8_EL2(6)
#define ICH_LR15_EL2			__LR8_EL2(7)

#define __AP0Rx_EL2(x)			sys_reg(3, 4, 12, 8, x)
#define ICH_AP0R0_EL2			__AP0Rx_EL2(0)
#define ICH_AP0R1_EL2			__AP0Rx_EL2(1)
#define ICH_AP0R2_EL2			__AP0Rx_EL2(2)
#define ICH_AP0R3_EL2			__AP0Rx_EL2(3)

#define __AP1Rx_EL2(x)			sys_reg(3, 4, 12, 9, x)
#define ICH_AP1R0_EL2			__AP1Rx_EL2(0)
#define ICH_AP1R1_EL2			__AP1Rx_EL2(1)
#define ICH_AP1R2_EL2			__AP1Rx_EL2(2)
#define ICH_AP1R3_EL2			__AP1Rx_EL2(3)

#ifndef __ASSEMBLY__

#include <linux/stringify.h>
#include <asm/barrier.h>

#define read_gicreg			read_sysreg_s
#define write_gicreg			write_sysreg_s

/*
 * Low-level accessors
 *
 * These system registers are 32 bits, but we make sure that the compiler
 * sets the GP register's most significant bits to 0 with an explicit cast.
 */

static inline void gic_write_eoir(u32 irq)
{
	write_sysreg_s(irq, ICC_EOIR1_EL1);
	isb();
}

static inline void gic_write_dir(u32 irq)
{
	write_sysreg_s(irq, ICC_DIR_EL1);
	isb();
}

static inline u64 gic_read_iar_common(void)
{
	u64 irqstat;

	irqstat = read_sysreg_s(ICC_IAR1_EL1);
	dsb(sy);
	return irqstat;
}

/*
 * Cavium ThunderX erratum 23154
 *
 * The gicv3 of ThunderX requires a modified version for reading the
 * IAR status to ensure data synchronization (access to icc_iar1_el1
 * is not sync'ed before and after).
 */
static inline u64 gic_read_iar_cavium_thunderx(void)
{
	u64 irqstat;

	asm volatile(
		"nop;nop;nop;nop\n\t"
		"nop;nop;nop;nop");

	irqstat = read_sysreg_s(ICC_IAR1_EL1);

	asm volatile(
		"nop;nop;nop;nop");
	mb();

	return irqstat;
}

static inline void gic_write_pmr(u32 val)
{
	write_sysreg_s(val, ICC_PMR_EL1);
}

static inline void gic_write_ctlr(u32 val)
{
	write_sysreg_s(val, ICC_CTLR_EL1);
	isb();
}

static inline void gic_write_grpen1(u32 val)
{
	write_sysreg_s(val, ICC_GRPEN1_EL1);
	isb();
}

static inline void gic_write_sgi1r(u64 val)
{
	write_sysreg_s(val, ICC_SGI1R_EL1);
}

static inline u32 gic_read_sre(void)
{
	return read_sysreg_s(ICC_SRE_EL1);
}

static inline void gic_write_sre(u32 val)
{
	write_sysreg_s(val, ICC_SRE_EL1);
	isb();
}

static inline void gic_write_bpr1(u32 val)
{
	write_sysreg_s(val, ICC_BPR1_EL1);
}

#define gic_read_typer(c)		readq_relaxed_no_log(c)
#define gic_read_irouter(c)		readq_relaxed_no_log(c)
#define gic_write_irouter(v, c)		writeq_relaxed_no_log(v, c)

#endif /* __ASSEMBLY__ */
#endif /* __ASM_ARCH_GICV3_H */
