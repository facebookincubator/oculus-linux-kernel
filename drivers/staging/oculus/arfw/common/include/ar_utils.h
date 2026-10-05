/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_utils.h
 *
 * @brief Common utility macros available across all contexts
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

/**
 * @file
 * Common utility macros available across all contexts
 */

#pragma once

/**
 * Calculate the ceiling value of `a` divided by `b`. See
 * http://www.cs.nott.ac.uk/~psarb2/G51MPC/slides/NumberLogic.pdf
 */
#define AR_CEILING_DIVIDE(a, b) (((a) + ((b)-1)) / (b))

/// Concatenate two source expressions.
#define AR_CONCAT(x, y) AR_CONCAT_A(x, y)

/// Concatenate two source tokens.
#define AR_CONCAT_A(x, y) x##y

/// Round `a` up to the nearest integer multiple of `b`.
#define AR_ROUNDUP(a, b) ((((a) + ((b)-1)) / (b)) * (b))

/// Round `a` down to the nearest integer multiple of `b`.
#define AR_ROUNDDOWN(a, b) (((a) / (b)) * (b))

/// Compute the offset to add to `a` which will align it to a block size `b`.
#define AR_ALIGNED_OFFSET(a, b) ((a)-AR_ROUNDDOWN(a, b))

/// Compute the number of elements in an array.
#define AR_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/// Number of bits in a uint8_t. By definition, there are 8.
#define AR_BITS_PER_UINT8 8

/// Number of bits in a uint16_t. By definition, there are 16.
#define AR_BITS_PER_UINT16 16

/// Number of bits in a uint32_t. By definition, there are 32.
#define AR_BITS_PER_UINT32 32

/// Number of bits in a uint64_t. By definition, there are 64.
#define AR_BITS_PER_UINT64 64

/// Convert a virtual address to a pointer.
#define AR_VA_TO_PTR(x) ((void*)(uintptr_t)(x))

/// Convert a pointer to a virtual address.
#define AR_PTR_TO_VA(x) ((AR_va_t)(uintptr_t)(x))

/// Constant value of 1 KiB (1024 or 2^10 bytes).
#define AR_KB UINT64_C(1024)

/// Constant value of 1 MiB (1024^2 or 2^20 bytes).
#define AR_MB (AR_KB * 1024)

/// Constant value of 1 GiB (1024^3 or 2^30 bytes).
#define AR_GB (AR_MB * 1024)

/// Poison constant for 4 bytes.
#define AR_POISON4 UINT32_C(0xDEADBEEF)

/// Poison constant for 8 bytes.
#define AR_POISON8 UINT64_C(0xDEADBEEFDEADBEEF)

/// Poison constant for an allocated, but uninitialized byte.
#define AR_MEMORY_ALLOC_POISON UINT8_C(0xB5)

/// Poison constant for a freed byte of memory.
#define AR_MEMORY_FREE_POISON UINT8_C(0xFA)

/// Convert a source expression to a literal string.
#define AR_TO_STRING(x) AR_STRINGIFY(x)

/// Convert a source token to a literal string.
#define AR_STRINGIFY(x) #x

/**
 * To enable -Wformat-pedantic %p in a printf-like format string
 * must have args that are cast to void* to avoid the type mismatch warning.
 *
 * Example usage:
 *   foo_t* foo = get_foo();
 *   printf("The foo = %p\n", AR_FMT_PTR(foo));
 */
#define AR_FMT_PTR(p) ((void*)(uintptr_t)(p))

/// Format a const pointer to the printf-style. See AR_FMT_PTR(p).
#define AR_FMT_CPTR(const_p) ((const void*)(const_p))

/// Align an address `a` to a block size of `b`.
#define AR_ALIGN(a, b) AR_ROUNDUP(a, b)

/// Check if an address `a` is aligned to a block size `b`.
#define AR_IS_ALIGNED(a, b) (!(((uintptr_t)(a)) & (((uintptr_t)(b)) - 1)))

/// Number of bits in a byte. By definition, there are 8.
#define AR_BITS_PER_BYTE (8)
