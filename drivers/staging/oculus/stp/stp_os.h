/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 */

#ifndef STP_LINUX_H
#define STP_LINUX_H

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/spi/spi.h>
#include <linux/kthread.h>
#include <linux/memblock.h>
#include <linux/err.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/random.h>
#include <uapi/linux/sched/types.h>

// Types
#define U8 u8
#define UINT unsigned int

// Mutex
#define STP_LOCK_TYPE struct mutex
#define STP_LOCK_INIT(m) mutex_init(&(m))
#define STP_LOCK(m) mutex_lock(&(m))
#define STP_UNLOCK(m) mutex_unlock(&(m))

// Alloc
#define STP_MALLOC(n) kzalloc(n, GFP_KERNEL)
#define STP_FREE kfree

// Log
#ifdef CONFIG_STP_DEBUG
#define STP_LOG pr_err
#define STP_LOG_INFO pr_err
#define STP_LOG_DEBUG pr_err
#define STP_LOG_ERROR pr_err
#else
#define STP_DONT_LOG(...)
#define STP_LOG pr_err
#define STP_LOG_INFO STP_DONT_LOG
#define STP_LOG_DEBUG STP_DONT_LOG
#define STP_LOG_ERROR pr_err
#endif

//Delay
#define STP_DELAY(t)  usleep_range(t, t + 1)

// Timeout values
#define STP_SMALL_DELAY 10
#define STP_TX_DELAY	100
#define STP_RX_DELAY	100
#define STP_ACK_DELAY	100
#define STP_RX_TIMEOUT	10000
#define STP_TX_TIMEOUT	10000
#define STP_ACK_TIMEOUT	1000

#define STP_NL	'\n'

#define STP_ASSERT(c, m)			\
	do { if (c) break;			\
		pr_err("STP Assert " m "\n");	\
	} while (0)

#endif
