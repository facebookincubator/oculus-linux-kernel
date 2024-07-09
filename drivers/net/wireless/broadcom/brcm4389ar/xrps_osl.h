/* SPDX-License-Identifier: GPL-2.0 */
/******************************************************************************
 *
 * @file xrps_osl.h
 * @brief OS layer interfaces for XRPS
 *
 * @details Ex: timers, workqueue, print. Their implementation are defined per
 * OS in xrps_<OS_NAME>.c.
 *
 *****************************************************************************/

#ifndef XRPS_OSL_H
#define XRPS_OSL_H

#ifdef CONFIG_XRPS

#ifndef __linux__
// Redefinition in linux/types.h
#include <stdint.h>
#endif

#if defined(__ZEPHYR__)

#include <kernel.h>
#include <logging/log.h>

#define XRPS_LOG_DECLARE() LOG_MODULE_DECLARE(xrps, CONFIG_XRPS_LOG_LEVEL)

#define XRPS_LOG_DBG LOG_DBG
#define XRPS_LOG_INF LOG_INF
#define XRPS_LOG_WRN LOG_WRN
#define XRPS_LOG_ERR LOG_ERR

typedef struct k_spinlock xrps_osl_spinlock_t;
typedef int xrps_osl_spinlock_flag_t;

#elif defined(__linux__)

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/kernel.h>
#include <linux/spinlock_types.h>

#define XRPS_LOG_DECLARE()

#define XRPS_LOG_DBG pr_debug
#define XRPS_LOG_INF pr_info
#define XRPS_LOG_WRN pr_warn
#define XRPS_LOG_ERR pr_err

#ifndef MAX
#define MAX(a, b) max(a, b)
#endif

typedef spinlock_t xrps_osl_spinlock_t;
typedef unsigned long xrps_osl_spinlock_flag_t;

#endif

// Advance declaration
struct xrps;

struct xrps_osl_intf {
	int (*init_osl)(struct xrps *xrps);
	int64_t (*get_ktime)(void);
	uint64_t (*ktime_to_us)(int64_t ktime);
	void (*cleanup_osl)(struct xrps *xrps);
	int (*start_sysint_timer)(uint64_t time);
	int (*stop_sysint_timer)(void);
	int (*submit_eot_work)(void);
	void (*spin_lock_init)(xrps_osl_spinlock_t *lock);
	xrps_osl_spinlock_flag_t (*spin_lock)(xrps_osl_spinlock_t *lock);
	void (*spin_unlock)(xrps_osl_spinlock_t *lock,
			    xrps_osl_spinlock_flag_t flags);
};

// Define this per OS in xrps_<OS NAME>.c
int xrps_init_osl_intf(struct xrps_osl_intf **osl_intf);

#endif /* CONFIG_XRPS */

#endif /* XRPS_OSL_H */
