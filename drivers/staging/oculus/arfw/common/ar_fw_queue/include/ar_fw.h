/*
 * This software contains information and intellectual property
 * that is confidential and proprietary to Meta, Inc. and its affiliates.
 */

/** @file */

#pragma once

#include <ar_common.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * IO request as read from the data queue
 */
typedef struct ar_fw_io_request {
  /**
   * Pointer to the raw data, still resident in the queue. Copy if needed
   * beyone the callback context
   */
  void* data;

  /// Size of the data, in bytes
  size_t data_size;

  /// Index of the data in the circular queue
  uint32_t index;
} ar_fw_io_request_t;

#ifdef __cplusplus
} // extern "C"
#endif
