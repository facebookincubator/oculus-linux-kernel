/*
 * This software contains information and intellectual property
 * that is confidential and proprietary to Facebook, Inc. and its affiliates.
 */

/** @file */

#pragma once

#include <ar_common.h>

#include "ar_fw.h"
#include "ar_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t queue_direction_t;

/// Type for ar firmware queue metadata
typedef struct {
  /// Total size of the memory used in the queue communication
  uint64_t total_mem_size;
  /// Size of metadata used for produce region of the queue
  uint64_t produce_meta_size;
  /// Size of metadata used for consume region of the queue
  uint64_t consume_meta_size;
  /// Size of the data region (actual payload data)
  uint64_t data_size;
  // Is this a transmit queue (client posts data to driver) or a receive queue
  /// (see enum queue_direction in ar_common.h)
  queue_direction_t queue_direction;
  /// Size of the queue elements
  uint32_t element_size;
  /// Number of elements in the queue
  uint16_t depth;
  // platform data cache line size
  uint32_t dcache_line_size;
  // platform page size
  uint32_t page_size;
} ar_fw_queue_meta_t;

/**
 * Return the size of the metadata region.
 *
 * @retval Size of the queue meta region.
 */
static inline size_t ar_fw_queue_meta_size(ar_fw_queue_meta_t* meta) {
  return meta->produce_meta_size + meta->consume_meta_size;
}

/// Default queue depth assumes limited number of outstanding messages
static const uint16_t c_ar_fw_default_queue_depth = 16;

/**
 * Initialize the queue metadata for a queue
 *
 * @param[in] depth Queue depth
 * @param[in] element_size the data size of elements in the queue
 * @param[in] queue_direction SEND if a send queue, RECEIVE if a receive queue
 * @param[in] dcache_line_size platform data cache line size.
 * @param[in] page_size platform page size
 * @param[out] meta The queue metadata
 *
 * @retval 0 On success.
 * @retval -1 bad element size (conflicts with dcache line size or page size)
 */
int ar_fw_queue_meta_init(
    uint16_t depth,
    uint32_t element_size,
    queue_direction_t queue_direction,
    uint32_t dcache_line_size,
    uint32_t page_size,
    ar_fw_queue_meta_t* meta);

/**
 * Callback invoked when a new request is received on queue.
 *
 * @param[in] callback_context Callback context registered during queue creation.
 * @param[in] request Pointer to ar_fw_io_request_t
 * @param[in] request_context Request context passed to the
 * request_callback during the poll call.
 * @param[out] mark_consumed True if the slot is ready to free. False if
 * ar_fw_queue_mark_slot_ready will be called later by data consumer.
 *
 * @retval TRUE if polling should continue
 */
typedef bool (*ar_fw_request_callback_t)(
    void* callback_context,
    ar_fw_io_request_t* request,
    void* request_context,
    bool* mark_consumed);

/// Whether a driver or client is creating this queue
typedef uint8_t queue_create_creator_t;
enum queue_create_creator {
  DRIVER,
  CLIENT,
};

/// Structure that describes the queue create.
typedef struct ar_fw_queue_creator_data {
  /// indicates queue creator: either driver or client.
  queue_create_creator_t queue_creator;

  /// request callback into to the on IO arrival.
  ar_fw_request_callback_t request_callback;

  /// context passed to the callback.
  void* callback_context;
} ar_fw_queue_creator_data_t;

/**
 * Queue create parameters initialization for the driver
 *
 * @param[out] creator_data The parameter structure to fill
 * @param[in] request_callback Optional on a receive queue. The callback invoked
 * on data arrival.
 * @param[in] callback_context Optional context passed when the callback is
 * invoked
 */
static inline void ar_fw_queue_creator_data_driver_init(
    ar_fw_queue_creator_data_t* creator_data,
    ar_fw_request_callback_t request_callback,
    void* callback_context) {
  *creator_data = (ar_fw_queue_creator_data_t){
      .queue_creator = DRIVER,
      .request_callback = request_callback,
      .callback_context = callback_context};
}

/**
 * Queue create parameters initialization for the client
 *
 * @param[out] creator_data The parameter structure to fill
 * @param[in] request_callback Optional on a transmit queue. The callback
 * invoked on data arrival.
 * @param[in] callback_context Optional context passed when the callback is
 * invoked
 */
static inline void ar_fw_queue_creator_data_client_init(
    ar_fw_queue_creator_data_t* creator_data,
    ar_fw_request_callback_t request_callback,
    void* callback_context) {
  *creator_data = (ar_fw_queue_creator_data_t){
      .queue_creator = CLIENT,
      .request_callback = request_callback,
      .callback_context = callback_context};
}

/// Type for ar firmware queue
typedef struct ar_fw_queue {
  /// Number of queue entries
  uint16_t size;

  /// size of each element
  uint32_t submit_element_size;

  /// Queue used for submission of data payloads
  ar_queue_t submit_queue;

  /// Queue creation parameters.
  ar_fw_queue_creator_data_t params;
} ar_fw_queue_t;

/**
 * Interface used to reserve the next slot in the queue
 *
 * @param[in] queue The AR firmware queue target
 * @param[out] io_request The resulting io request data
 *
 * @retval 0 if a slot was allocated
 */
int ar_fw_queue_slot_reserve(ar_fw_queue_t* queue, ar_fw_io_request_t* io_request);

/**
 * Interface used to mark an existing slot allocation ready for consumption
 *
 * @param[in] queue The AR firmware queue target
 * @param[in] io_request the io request to produce
 *
 * @retval True if successful
 *
 */
bool ar_fw_queue_slot_produce(ar_fw_queue_t* queue, ar_fw_io_request_t* io_request);

/**
 * Create a queue, with disjoint metadata and data regions.
 *
 * @param[in] meta Queue metadata.
 * @param[in] meta_mem_base Base memory address queue meta region.
 * @param[in] meta_mem_size Size in bytes of meta region.
 * @param[in] data_mem_base Base memory address queue data region.
 * @param[in] data_mem_size Size in bytes of data region.
 * @param[in] params Create params.
 * @param[out] result The new queue object. Memory is allocated by the caller.
 *
 * @retval 0 On success
 * @retval -1 If queue meta is invalid.
 */
int ar_fw_queue_create(
    const ar_fw_queue_meta_t* meta,
    uintptr_t meta_mem_base,
    uint64_t meta_mem_size,
    uintptr_t data_mem_base,
    uint64_t data_mem_size,
    const ar_fw_queue_creator_data_t* params,
    ar_fw_queue_t* result);

/**
 * Interface used to replace the default callback
 *
 * @param[in] queue The queue object.
 * @param[in] request_callback_override The new callback to be invoked on data arrival
 * @param[in] context_override The new context to associate with the callback
 *
 * @retval true if successfully set
 */
bool ar_fw_queue_set_callback(
    ar_fw_queue_t* queue,
    ar_fw_request_callback_t request_callback_override,
    void* context_override);

/**
 * Destroy the queue.
 *
 * @param[in] queue The queue object.
 *
 * @retval 0 The operation completed successfully.
 */
int ar_fw_queue_destroy(ar_fw_queue_t* queue);

/**
 * Mark a slot as consumed. Use only if calling ar_fw_queue_poll
 * with mark_consumed as false.
 *
 * @param[in] queue The queue object.
 * @param[in] index The index to mark as complete from the
 * ar_fw_io_request_t
 *
 * @retval True if successful
 *
 */
bool ar_fw_queue_mark_slot_ready(ar_fw_queue_t* queue, uint32_t index);

/**
 * Poll the N (count) ar firmware queue.
 *
 * @param[in] queue The queue object.
 * @param[in] count Max number of queue entries processed during this call.
 * @param[in] caller_request The context passed to completion callbacks
 *
 * @retval Number of completions that were polled.
 */
size_t ar_fw_queue_poll(ar_fw_queue_t* queue, size_t count, void* caller_request);

/**
 * Get the base address of the queue data elements
 *
 * @param[in] queue The results of ar_fw_queue_create
 *
 * @return The VM address of the first element in the queue
 */
uintptr_t ar_fw_queue_base_address_get(ar_fw_queue_t* queue);

/**
 * Get the number of items that can be consumed from an ar firmware queue
 *
 * @param[in] queue The queue object.
 *
 * @return the number of items that can be consumed from the queue
 */
uint16_t ar_fw_queue_get_consumable_count(const ar_fw_queue_t* queue);

#ifdef __cplusplus
} // extern "C"
#endif
