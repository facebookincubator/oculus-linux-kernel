/*
 * This software contains information and intellectual property
 * that is confidential and proprietary to Meta, Inc. and its affiliates.
 */

/** @file */

#pragma once

#include "ar_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculates the next consume counter, if the consume element count
 * could be expanded, update the consume element count and return the
 * adjusted next consume counter.
 *
 * @param[in] queue Circular queue to get the next consume counter.
 *
 * @return Next consume counter value.
 */
long ar_queue_calculate_next_consumed_counter(ar_queue_t* queue);

/**
 * Checks if current consume element index is available to be consumed.
 * If it is, increments the consumed counter and returns a pointer to the
 * element.
 *
 * @note This function can be called from the consumer, but also from the
 *       producer to drain the queue during tear down.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[out] element_idx The index of the element returned. Only valid
 *                              when NULL is not returned.
 *
 * @return
 *    NULL                      No elements to consume
 *    Pointer                   Pointer to element if successful
 */
void* ar_queue_get_next_consume(ar_queue_t* queue, uint32_t* element_idx);

/**
 * Marks an element as produce ready. Can only be called by consumer.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[in] element_idx The index of the element to mark producable.
 *
 * @retval AR_OK Element marked successfully, error otherwise
 */
ar_status_t ar_queue_mark_element_produce_ready(ar_queue_t* queue, int element_idx);

/**
 * Checks if a packet is available to be consumed.
 *
 * @note This function can be called from the consumer, but also from the
 *       producer to check whether whether we may go to sleep and wait
 *       for a new packet.
 *
 * @param[in] queue The structure defining a circular queue.
 *
 * @return True if a packet is available,
 *         False otherwise.
 */
bool ar_queue_packet_available(ar_queue_t* queue);

#ifdef __cplusplus
} // extern "C"
#endif
