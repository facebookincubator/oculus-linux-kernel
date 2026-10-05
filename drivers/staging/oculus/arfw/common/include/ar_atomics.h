/* SPDX-License-Identifier: GPL-2.0 */
/*******************************************************************************
 * @file ar_atomics.h
 *
 * @brief Common atomics macros available across all contexts
 *
 ********************************************************************************
 * Copyright (c) Meta, Inc. and its affiliates. All Rights Reserved
 *******************************************************************************/

/**
 * @file
 * Common atomics macros available across all contexts
 */

#pragma once

#include "ar_common.h"

/* Memory barriers */
#if defined(__ARM_ARCH_7A__)
#define ar_smp_read_mb() __asm__ volatile("dmb" : : : "memory")
#elif defined(__arm__) || defined(__aarch64__) || defined(__ARM_ARCH_8A__)
#define ar_smp_read_mb() __asm__ volatile("dmb ishld" : : : "memory")
#elif defined(__i386__) || defined(__x86_64__)
#define ar_smp_read_mb() asm volatile("lfence" : : : "memory")
#endif

// Allign with the kernel view and see them as signed int and long respecively
#define ar_atomic_t int
#define ar_atomic64_t long

typedef enum ar_memory_order {
  AR_MEMORY_ORDER_RELAXED = __ATOMIC_RELAXED,
  AR_MEMORY_ORDER_CONSUME = __ATOMIC_CONSUME,
  AR_MEMORY_ORDER_ACQUIRE = __ATOMIC_ACQUIRE,
  AR_MEMORY_ORDER_RELEASE = __ATOMIC_RELEASE,
  AR_MEMORY_ORDER_ACQ_REL = __ATOMIC_ACQ_REL,
  AR_MEMORY_ORDER_SEQ_CST = __ATOMIC_SEQ_CST
} ar_memory_order_t;

/**
 * Performs an atomic store operation.
 *
 * @param[in] ptr Pointer to the value to write.
 * @param[in] newval The new value to write into *ptr.
 * @param[in] order The memory order.
 */
static inline void ar_atomic_store(ar_atomic_t* ptr, int newval, ar_memory_order_t order) {
  __atomic_store_n(ptr, newval, (int)order);
}

/**
 * Performs an atomic store operation.
 *
 * @param[in] ptr Pointer to the value to write.
 * @param[in] newval The new value to write into *ptr.
 * @param[in] order The memory order.
 */
static inline void ar_atomic64_store(ar_atomic64_t* ptr, long newval, ar_memory_order_t order) {
  __atomic_store_n(ptr, newval, (int)order);
}

/**
 * Performs an atomic load operation.
 *
 * @param[in] ptr Pointer to the value to load.
 * @param[in] order The memory order.
 * @return The value stored in *ptr.
 */
static inline int ar_atomic_load(ar_atomic_t const* ptr, ar_memory_order_t order) {
  return __atomic_load_n(ptr, (int)order);
}

/**
 * Performs an atomic load operation.
 *
 * @param[in] ptr Pointer to the value to load.
 * @param[in] order The memory order.
 * @return The value stored in *ptr.
 */
static inline long ar_atomic64_load(ar_atomic64_t const* ptr, ar_memory_order_t order) {
  return __atomic_load_n(ptr, (int)order);
}

/**
 * Performs an atomic compare and exchange operation. The contents of *ptr are
 * compared with the contents of *oldval. If they are equal, newval is written
 * into *ptr. Otherwise, the current contents of *ptr are written to *oldval.
 *
 * @param[in,out] ptr Pointer to the value to exchange with.
 * @param[in,out] oldval Pointer value to compare against.
 * If this function returns false, *oldval is overwritten with
 * the current value of the atomic, otherwise it is unchanged.
 * @param[in] newval The value to write to *ptr.
 * @param[in] success_order The memory order to use if the exchange succeeds
 * @param[in] fail_order The memory order to use if the exchange fails
 * @return True if newval is written into *ptr.
 */
static inline bool ar_atomic_compare_exchange(
    ar_atomic_t* ptr,
    int* oldval,
    int newval,
    ar_memory_order_t success_order,
    ar_memory_order_t fail_order) {
  return __atomic_compare_exchange_n(
      ptr, oldval, newval, false, (int)success_order, (int)fail_order);
}

/**
 * Performs an atomic compare and exchange operation. The contents of *ptr are
 * compared with the contents of *oldval. If they are equal, newval is written
 * into *ptr. Otherwise, the current contents of *ptr are written to *oldval.
 *
 * @param[in,out] ptr Pointer to the value to exchange with.
 * @param[in,out] oldval Pointer value to compare against.
 * If this function returns false, *oldval is overwritten with
 * the current value of the atomic, otherwise it is unchanged.
 * @param[in] newval The value to write to *ptr.
 * @param[in] success_order The memory order to use if the exchange succeeds
 * @param[in] fail_order The memory order to use if the exchange fails
 * @return True if newval is written into *ptr.
 */
static inline bool ar_atomic64_compare_exchange(
    ar_atomic64_t* ptr,
    long* oldval,
    long newval,
    ar_memory_order_t success_order,
    ar_memory_order_t fail_order) {
  return __atomic_compare_exchange_n(
      ptr, oldval, newval, false, (int)success_order, (int)fail_order);
}

/**
 * Atomically computes *ptr (add) val, and stores the result in *ptr.
 *
 * @param[in,out] ptr Pointer to the first operand. Result will be stored here.
 * @param[in] val The second operand.
 * @param[in] order The memory order.
 * @return The value previously at *ptr.
 */
static inline int ar_atomic_fetch_add(ar_atomic_t* ptr, int val, ar_memory_order_t order) {
  return __atomic_fetch_add(ptr, val, (int)order);
}

/**
 * Atomically computes *ptr (add) val, and stores the result in *ptr.
 *
 * @param[in,out] ptr Pointer to the first operand. Result will be stored here.
 * @param[in] val The second operand.
 * @param[in] order The memory order.
 * @return The value previously at *ptr.
 */
static inline long ar_atomic64_fetch_add(ar_atomic64_t* ptr, long val, ar_memory_order_t order) {
  return __atomic_fetch_add(ptr, val, (int)order);
}
