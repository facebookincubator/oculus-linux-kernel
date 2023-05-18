/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013-2014, 2017, 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __QPNP_MISC_H
#define __QPNP_MISC_H

#include <linux/errno.h>

enum twm_state {
	PMIC_TWM_CLEAR,
	PMIC_TWM_ENABLE,
};

#ifdef CONFIG_QPNP_MISC
/**
 * qpnp_misc_irqs_available - check if IRQs are available
 *
 * @consumer_dev: device struct
 *
 * This function returns true if the MISC interrupts are available
 * based on a check in the MISC peripheral revision registers.
 *
 * Any consumer of this function needs to reference a MISC device phandle
 * using the "qcom,misc-ref" property in their device tree node.
 */

int qpnp_misc_irqs_available(struct device *consumer_dev);

/**
 * qpnp_misc_read_reg - read register from misc device
 *
 * @node: device node pointer
 * @address: address offset in misc peripheral to be read
 * @val: data read from register
 *
 * This function returns zero if reading the MISC register succeeds.
 *
 */

int qpnp_misc_read_reg(struct device_node *node, u16 addr, u8 *val);
/**
 * qpnp_misc_twm_notifier_register - register to the twm mode notifier
 *
 * @nb: pointer to the client's notifier handle
 *
 * This function returns 0 if the client is successfully added to the
 * notifer list.
 */
int qpnp_misc_twm_notifier_register(struct notifier_block *nb);

/**
 * qpnp_misc_twm_notifier_unregister - unregister to the twm mode notifier
 *
 * @nb: pointer to the client's notifier handle
 *
 * This function returns 0 if the client is successfully removed from the
 * notifer list.
 */
int qpnp_misc_twm_notifier_unregister(struct notifier_block *nb);
#else
static inline int qpnp_misc_irqs_available(struct device *consumer_dev)
{
	return 0;
}
static inline int qpnp_misc_read_reg(struct device_node *node, u16 addr,
					u8 *val)
{
	return 0;
}
static inline int qpnp_misc_twm_notifier_register(struct notifier_block *nb)
{
	return 0;
}
static inline int qpnp_misc_twm_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}
#endif
#endif
