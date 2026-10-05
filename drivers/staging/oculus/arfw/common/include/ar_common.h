/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_common.h
 *
 * @brief Definitions needed by the arfirmware code in order to be compiled both
 * in user-level code and in the kernel.
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

#ifndef AR_COMMON_H
#define AR_COMMON_H

#ifndef __KERNEL__
#include <android/log.h>
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define AR_ASSERT(x)                                                                     \
  do {                                                                                   \
    if (!(x)) {                                                                          \
      __android_log_assert("assert", "ARFIRMWARE", "%s:%d: %s", __FILE__, __LINE__, #x); \
    }                                                                                    \
  } while (0)

#define EXPORT_SYMBOL(x)

#ifndef __has_extension
#define AR_HAS_EXTENSION(x) 0
#else // !__has_extension
#define AR_HAS_EXTENSION(x) __has_extension(x)
#endif // __has_extension

#if AR_HAS_EXTENSION(nullability)
#define AR_NONNULL _Nonnull
#define AR_NULLABLE _Nullable
#else // !AR_HAS_EXTENSION(nullability)
#define AR_NONNULL
#define AR_NULLABLE
#endif // AR_HAS_EXTENSION(nullability)

#else // __KERNEL__

#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#define AR_ASSERT(cond) BUG_ON(!(cond))

#define UINT16_MAX U16_MAX
#define UINT32_MAX U32_MAX
#define UINT8_C(x) (x)
#define UINT32_C(x) (x##U)
#define UINT64_C(x) (x##ULL)

#endif // !__KERNEL__

// Describes return codes.
typedef enum {
  AR_OK = 0,
  AR_ERR_BAD_ALIGNMENT,
  AR_ERR_BAD_STATE,
  AR_ERR_INTERNAL,
  AR_ERR_INTERRUPTED,
  AR_ERR_INVALID_ARGS,
  AR_ERR_MAX_VALUE,
  AR_ERR_NO_MEMORY,
  AR_ERR_NOT_IMPLEMENTED,
  AR_ERR_SHUTDOWN,
  AR_ERR_TIMEDOUT,
} ar_status_t;

// Describes the direction of a queue.
enum queue_direction {
  SEND,
  RECEIVE,
};

#define AR_LIKELY(x) __builtin_expect((x), 1)
#define AR_UNLIKELY(x) __builtin_expect((x), 0)

#endif // AR_COMMON_H
