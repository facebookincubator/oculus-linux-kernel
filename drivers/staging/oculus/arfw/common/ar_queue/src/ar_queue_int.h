/*
 * This software contains information and intellectual property
 * that is confidential and proprietary to Meta, Inc. and its affiliates.
 */

/** @file */

#pragma once

#include "ar_queue.h"

#include <ar_atomics.h>

/// Get the ownership count.
#define AR_COUNT_TO_MASKCOUNT(x, s) (((x) + (s)-1) / (s))
#define AR_OWNERSHIP_BITS (64)

/** Extra metadata that will be added to each circular queue packet. */
typedef struct {
  /**
   * The key to store information about the packet regarding production
   * and consumption
   */
  /* Current circular iteration around the queue */
  ar_atomic_t iteration;
} ar_queue_packet_meta_t;

typedef union {
  long value;
  ar_atomic64_t atomic;
} ar_queue_ownership_t;

typedef union {
  ar_queue_consumer_info_t consumed;

  /** Atomic value */
  ar_atomic64_t atomic;

  /** long value */
  long value;
} ar_queue_consumed_t;

typedef struct ar_queue_consume {
  /* Counter that indicates where the next element can consumed from */
  ar_queue_consumed_t consumed_counter;

  /* Number of elements in the circular queue */
  ar_atomic_t element_count;

  // 4-byte padding
  uint8_t pad[4];

  /* Size in bytes of each element in the circular queue */
  size_t element_size;

  /**
   * Consumer mapped pointers to packet runs and packet ownership.
   *
   * The ownership array indicates if an element is owned by producer or
   * consumer. Each element's ownership will be represented by a bit.
   * 1 = element owned by consumer, 0 = element owned by producer.
   */
  uint64_t ownership[];
} ar_queue_consume_t;

typedef union {
  ar_queue_producer_info_t generic;

  /** Atomic value */
  ar_atomic64_t atomic;

  /** long value */
  long value;
} ar_queue_produced_t;

typedef struct ar_queue_produce {
  /* Number of elements in the circular queue */
  ar_atomic_t element_count;

  /*
   * This is the pointer to the actual packet data.
   */
  uint8_t* data;

  /* Size in bytes of each element in the circular queue */
  size_t element_size;

  /* Counters to keep track of data related to the producer */
  ar_queue_produced_t produced_counter;

  /*
   * The data in the circular queue.
   *
   * This contains the packet metadata and virtual address array
   * of runs in that order.
   */
  uint8_t metadata[];
} ar_queue_produce_t;

/**
 * Returns the packet meta struct for a queue.
 *
 * @brief packet meta data is stashed at the beginning of
 * the produce data section.
 *
 * @param[in] data Pointer to the ar queue packets.
 * @param[in] index The index of the packet meta to get.
 *
 * @return Pointer to ar_queue_packet_meta_t.
 */
static inline ar_queue_packet_meta_t* ar_queue_get_packet_meta(void* data, uint32_t index) {
  return (
      ar_queue_packet_meta_t*)(uintptr_t)((uint8_t*)(data) + (sizeof(ar_queue_packet_meta_t) * index));
}

/**
 * Returns the element from the circular queue.
 *
 * @brief element array is stashed behind the packet meta data of
 * the produce data section.
 *
 * @param[in] queue Pointer to the base of runs.
 * @param[in] index The index of the element to return.
 *
 * @return Pointer to the packet.
 */
static inline void* ar_queue_get_packet(ar_queue_t* queue, uint32_t index) {
  uint8_t* elements = (uint8_t*)queue->data;
  uint32_t index_in_run = index % queue->element_count;

  return (void*)(uintptr_t)(elements + (queue->produce->element_size * index_in_run));
}

/**
 * Get ownership array of the packets.
 *
 * @param[in] queue The circular queue contains the ownership bitmap array.
 *
 * @return ownership bitmap array stored in consume of circular queue.
 */
static inline ar_queue_ownership_t* ar_queue_get_ownership(ar_queue_t* queue) {
  return (ar_queue_ownership_t*)(uintptr_t)queue->consume->ownership;
}

/**
 * @brief Checks if the ownership bit with the associated index matches the
 * desired ownership
 *
 * @param[in] idx The index of the element to check in the ownership array.
 * @param[in] ownership The ownership array.
 * @param[in] desired The desired ownership in the ownership array.
 *
 * @retval TRUE The ownership bit matches owner.
 * @retval FALSE The ownership bit does not match.
 */

bool ar_queue_check_ownership(
    uint32_t idx,
    ar_queue_ownership_t* ownership,
    ar_queue_owner_t desired);

/**
 * Flips ownership for the particular element.
 *
 * @param[in] element_idx The element index to flip the bit for.
 * @param[in] ownership The ownership array.
 * @param[in] owner If the caller is the producer or consumer.
 *
 * @retval AR_OK  Ownership changed.
 * @retval AR_ERR_BAD_STATE Ownership is already matching desired.
 */
ar_status_t ar_queue_flip_ownership(
    uint32_t element_idx,
    ar_queue_ownership_t* ownership,
    ar_queue_owner_t owner);

/**
 * Verifies the packet can be produced.
 *
 * Verifies the packet can be produced into by checking the iteration is
 * only one iteration behind.
 *
 * @param[in] iteration Desired iteration
 * @param[in] packet_iteration The iteration inside the packet
 *
 * @retval TRUE Packet can be produced into.
 * @retval FALSE Packet cannot be produced into, the packet hasn't been flipped
 * yet.
 */
static inline bool ar_check_produce_iteration(uint16_t iteration, uint16_t packet_iteration) {
  return iteration == (uint16_t)(packet_iteration + 1) % AR_QUEUE_ITERATION_COUNT;
}

/**
 * Increments the packet's circular iteration. This is expected to overflow, and
 * thus we suppress the sanitizers.
 *
 * @param[in] packet_meta The pointer to the packet meta information.
 */
void ar_increment_packet_iteration(ar_queue_packet_meta_t* packet_meta);

/**
 * @brief Check if packet_index is available to be produced to.
 *
 * To be allowed to produce to the packet_index, the packet iteration is
 * AR_QUEUE_PACKET_UNUSED or the packet iteration is
 * one iteration behind the queue_iteration and the ownership for the index
 * AR_QUEUE_PRODUCER.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[in] queue_iteration The iteration of the queue for this check.
 * @param[in] packet_index The packet index.
 * @param[in] packet_iteration The iteration corresponding to the packet
 *                               index.
 * @param[in] consumer_iteration The iteration of the consume metadata.
 *
 * @return True if packet_index is available to be produced to,
 *         False otherwise.
 */
bool ar_queue_can_produce_idx(
    ar_queue_t* queue,
    uint16_t queue_iteration,
    uint32_t packet_index,
    uint16_t packet_iteration,
    uint16_t consumer_iteration);

/**
 * Verifies if a packet is ready to be consumed.
 *
 * Verifies if a packet is ready to be consumed, by comparing the
 * iteration counter to the data in the packet. If the iteration does
 * not match, then another consumer thread is holding the packet and it should
 * not be given out again.
 *
 * @param[in] required_consume The required iteration counter & index.
 * @param[in] packet_meta The pointer to the packet meta information in the
 * queue.
 *
 * @retval TRUE Iterations match.
 * @retval FALSE Iterations does not match.
 */
bool ar_check_consume_packet_iteration(
    ar_queue_consumed_t required_consume,
    const ar_queue_packet_meta_t* packet_meta);
