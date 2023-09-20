/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * @file xrps_linux.h
 * @brief Linux-specific type definitions.
 *
 *****************************************************************************/
#ifndef XRPS_LINUX_H
#define XRPS_LINUX_H

#ifdef CONFIG_XRPS

#include <linux/hrtimer.h>
#include <linux/workqueue.h>
#include "xrps_osl.h"

struct xrps_linux {
	struct kobject *kobj;
	struct hrtimer sysint_timer; /* System interval timer */
	struct work_struct sysint_work;
	struct work_struct eot_work;
	struct xrps *xrps;
};

#endif /* CONFIG_XRPS */

#endif
