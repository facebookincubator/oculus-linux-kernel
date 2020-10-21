// SPDX-License-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/kernel.h>

static int __init hv_guest_init(void)
{
	pr_info("Hypervisor guest init\n");
	return 0;
}

static void __exit hv_guest_exit(void)
{
}

module_init(hv_guest_init);
module_exit(hv_guest_exit);

MODULE_DESCRIPTION("HYPERVISOR_GUEST");
MODULE_LICENSE("GPL v2");
