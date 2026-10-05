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

#ifndef AR_QUEUE_UPDATE
#define AR_QUEUE_UPDATE
#endif

typedef enum {
  /** element is owned by the consumer */
  AR_QUEUE_CONSUMER = 1,

  /** element is owned by the producer */
  AR_QUEUE_PRODUCER = 0,
} ar_queue_owner_t;

/*
 * Memory regions must be 8-byte aligned so that uint64 atomic
 * operations will work.
 */
#define AR_QUEUE_ALIGN_DEFAULT (sizeof(uint64_t))

// Maximum elements are allowed in circular queue.
#define AR_QUEUE_MAX_ELEMENTS (1 << 15)

// Initial packet iteration to represent the packet has not been used.
#define AR_QUEUE_PACKET_UNUSED (UINT16_MAX)

/*
 * Port iteration number range [0, UINT16_MAX - 1]. UINT16_MAX has
 * been reserved to be unused.
 */
#define AR_QUEUE_ITERATION_COUNT (UINT16_MAX)

/**
 * Information returned as an atomic snapshot for the consume side of a queue.
 */
typedef struct ar_queue_consumer_info {
  uint16_t reserved : 1;

  /* Counter that indicates where the next element can be consumed from */
  uint16_t index : 15;

  /* Current circular iteration around the queue */
  uint16_t iteration;

  uint32_t padding;
} ar_queue_consumer_info_t;

/**
 * Information returned as an atomic snapshot for the produce side of a queue.
 */
typedef struct ar_queue_producer_info {
  uint16_t unused;

  /* Current circular iteration around the queue */
  uint16_t iteration;

  /* Counter that indicates where the next element can be produced into */
  uint16_t index;

  /*
  Counter that indicates how many reserved elements are being used. These
  will still be reserved for the producer
  */
  uint16_t in_flight;
} ar_queue_producer_info_t;

typedef struct {
  /** Pointer to the produce portion of the circular queue. The pointer
   * will be in the struct's owner's address space. */
  struct ar_queue_produce* produce;

  /** Pointer to the consume portion of the circular queue. The pointer
   * will be in the struct's owner's address space. */
  struct ar_queue_consume* consume;

  /** Pointer to the start of the data in the circular queue. Any padding is
   * already accounted for by this pointer. */
  void* data;

  /** If the circular queue owner is the PRODUCER or CONSUMER */
  ar_queue_owner_t owner;

  /** The number of elements (num of packets). */
  uint16_t element_count;
} ar_queue_t;

typedef struct {
  /** Pointer to the produce section of the circular queue */
  void* produce_meta;

  /** Size (in bytes) of the produce section of the circular queue */
  size_t produce_size;

  /** Pointer to the consume section of the circular queue */
  void* consume_meta;

  /** Size (in bytes) of the consume section of the circular queue */
  size_t consume_size;

  /** Pointer to the shared data section of the circular queue */
  void* data;

  /** Size (in bytes) of the data section of the circular queue */
  size_t data_size;

  /** If the caller is the PRODUCER or CONSUMER */
  ar_queue_owner_t owner;

  /** The size of each element (in bytes). */
  size_t element_size;

  /** The number of elements. */
  uint16_t element_count;

  /** The alignment requirement for the first queue element.
   * See ar_queue_required_size */
  size_t align;

} ar_queue_setup_t;

/**
 * Creates a circular queue out of the allocated memory space that will be
 * shared with the kernel.
 *
 * @param[in] queue_setup Metadata related to setting up the queue.
 * @param[in,out] ar_queue Structure containing metadata for the
 *     circular queue
 *
 * @retval AR_OK Queue created successfully.
 * @retval AR_ERR_INVALID_ARGS Invalid or null queue_setup or circular_queue.
 */
ar_status_t ar_queue_create(ar_queue_setup_t* queue_setup, ar_queue_t* ar_queue);

/**
 * Returns if the circular queue is empty. This should only be called when
 * we can assure no new entries can be produced (the PRODUCER view into the
 * queue should be destroyed)
 *
 * @param[in] queue The circular queue.
 *
 * @return
 *      TRUE                    Circular queue is empty
 *      FALSE                   Circular queue is not empty
 */
bool ar_queue_is_empty(ar_queue_t* queue);

/**
 * Destroys an allocated circular queue.
 *
 * @param[in] queue Circular queue data structure.
 */
void ar_queue_destroy(ar_queue_t* queue);

/**
 * Calculates the size needed to create a circular queue.
 *
 * @param[in] element_size The size of each element in the circular
 * queue.
 * @param[in] element_count The number of elements in the circular
 * queue.
 * @param[in] align The client requirement for alignment of the first element
 * in the queue. Elements can be viewed as a packed array past this alignment
 * point. A value of CIRCULAR_QUEUE_ALIGN_DEFAULT may be used to specify no
 * additional alignment requirements.
 * @param[out] produce_meta_size The number of bytes that need to be
 * allocated for the produce metadata portion of the queue.
 * @param[out] consume_meta_size The number of bytes that need to be
 * allocated for the consume metadata portion of the queue.
 * @param[out] data_size The number of bytes that need to be
 * allocated for the data packet portion of the queue.
 *
 * @return
 *      AR_OK                   Queue can be created
 *      AR_ERR_INVALID_ARGS     Invalid element_size or element_count
 */
ar_status_t ar_queue_required_size(
    size_t element_size,
    uint16_t element_count,
    size_t align,
    size_t* produce_meta_size,
    size_t* consume_meta_size,
    size_t* data_size);

/// ar packet
typedef struct {
  uint64_t data[1];
} ar_packet_t;

/**
 * @brief Allocate a new ar packet to produce into.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[out] element_idx The index corresponding to the returned packet.
 *                           Only valid when NULL is not returned.
 *
 * @return
 *    NULL                   No elements available in the circular queue
 *    Pointer                Pointer to allocated packet if successful.
 */
ar_packet_t* ar_queue_get_next_produce(ar_queue_t* queue, uint32_t* element_idx);

/**
 * Marks an element as consume ready. Can only be called by producer.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[in] element_idx The index of the element to mark consumable
 *
 * @retval AR_OK Element marked successfully, error otherwise
 */
ar_status_t ar_queue_mark_element_consume_ready(ar_queue_t* queue, uint32_t element_idx);

/**
 * Dump the ar queue metadata into a printable string.
 *
 * @param[in] queue The ar_queue
 * @param[in] buf The target string buffer
 * @param[in] buflen The size of target buf
 *
 * @retval -1 if the output was truncated
 * @retval 0 on success
 */
int ar_queue_dump(ar_queue_t* queue, char* buf, size_t buflen);

/**
 * Get the ar queue consumer info.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Consumer info
 */
ar_queue_consumer_info_t ar_queue_get_consumer_info(const ar_queue_t* queue);

/**
 * Get the ar queue producer info.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Producer info
 */
ar_queue_producer_info_t ar_queue_get_producer_info(const ar_queue_t* queue);

/**
 * Get the ar queue consumer index.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Consumer index
 */
uint16_t ar_queue_get_consumer_idx(ar_queue_t* queue);

/**
 * Get the ar queue producer index.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Producer index
 */
uint16_t ar_queue_get_producer_idx(ar_queue_t* queue);

/**
 * Get the ar queue producer inflight.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Producer inflight
 */
uint16_t ar_queue_get_producer_inflight(ar_queue_t* queue);

/**
 * Get the ar queue consumer iteration.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Consumer iteration
 */
uint16_t ar_queue_get_consumer_iteration(ar_queue_t* queue);

/**
 * Get the ar queue producer iteration.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Producer iteration
 */
uint16_t ar_queue_get_producer_iteration(ar_queue_t* queue);

#ifdef __cplusplus
} // extern "C"
#endif
