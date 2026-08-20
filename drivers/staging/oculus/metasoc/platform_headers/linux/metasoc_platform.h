/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) Meta Platforms, Inc. and affiliates. */

// @MARK:COVERAGE_EXCLUDE_FILE

// header required for memcpy which is platform dependent
#include <linux/string.h>
#include <linux/printk.h>

/* Define MIN, MAX macros for local use, we want to keep
 * external dependencies to minimum in order to be able to
 * run in both linux and MCU
 */
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#define METASOC_LOG(...) pr_debug(__VA_ARGS__)
