/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/ptrace.h>

#include <asm/reg.h>

int machine_check_8xx(struct pt_regs *regs)
{
	unsigned long reason = regs->msr;

	pr_err("Machine check in kernel mode.\n");
	pr_err("Caused by (from SRR1=%lx): ", reason);
	if (reason & 0x40000000)
		pr_err("Fetch error at address %lx\n", regs->nip);
	else
		pr_err("Data access error at address %lx\n", regs->dar);

#ifdef CONFIG_PCI
	/* the qspan pci read routines can cause machine checks -- Cort
	 *
	 * yuck !!! that totally needs to go away ! There are better ways
	 * to deal with that than having a wart in the mcheck handler.
	 * -- BenH
	 */
	bad_page_fault(regs, regs->dar, SIGBUS);
	return 1;
#else
	return 0;
#endif
}
