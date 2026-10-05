#include "mock_circular_queue.h"

CircularQueueMock* g_cq_mock = nullptr;

/**
 * Creates a circular queue out of the allocated memory space that will be
 * shared with another user process.
 *
 * @param[in] queue_setup Metadata related to setting up the queue.
 * @param[in,out] ar_queue Structure containing metadata for the
 *     circular queue
 *
 * @retval AR_OK Queue created successfully.
 * @retval AR_ERR_INVALID_ARGS Invalid or null queue_setup or circular_queue.
 */
ar_status_t ar_queue_create(ar_queue_setup_t* queue_setup, ar_queue_t* ar_queue) {
  return g_cq_mock->CircularQueueCreate(queue_setup, ar_queue);
}

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
bool ar_queue_is_empty(ar_queue_t* queue) {
  return g_cq_mock->CircularQueueIsEmpty(queue);
}

/**
 * Destroys an allocated circular queue.
 *
 * @param[in] queue Circular queue data structure.
 */
void ar_queue_destroy(ar_queue_t* queue) {
  g_cq_mock->CircularQueueDestroy(queue);
}

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
    size_t* data_size) {
  return g_cq_mock->CircularQueueRequiredSize(
      element_size, element_count, align, produce_meta_size, consume_meta_size, data_size);
}

/**
 * @brief Allocate a new generic packet to produce into.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[out] element_idx The index corresponding to the returned packet.
 *                           Only valid when NULL is not returned.
 *
 * @return
 *    NULL                   No elements available in the circular queue
 *    Pointer                Pointer to allocated packet if successful.
 */
ar_packet_t* ar_queue_get_next_produce(ar_queue_t* queue, uint32_t* element_idx) {
  return g_cq_mock->CircularQueueGetNextProduce(queue, element_idx);
}

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
void* ar_queue_get_next_consume(ar_queue_t* queue, uint32_t* element_idx) {
  return g_cq_mock->CircularQueueGetNextConsume(queue, element_idx);
}

/**
 * Marks an element as consume ready. Can only be called by producer.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[in] element_idx The index of the element to mark consumable.
 */
ar_status_t ar_queue_mark_element_consume_ready(ar_queue_t* queue, uint32_t element_idx) {
  return g_cq_mock->CircularQueueMarkElementConsumeReady(queue, element_idx);
}

/**
 * Marks an element as produce ready. Can only be called by consumer.
 *
 * @param[in] queue The structure defining a circular queue.
 * @param[in] element_idx The index of the element to mark producable.
 */
ar_status_t ar_queue_mark_element_produce_ready(ar_queue_t* queue, int element_idx) {
  return g_cq_mock->CircularQueueMarkElementProduceReady(queue, element_idx);
}

/**
 * Get the ar queue consumer info.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Consumer info
 */
ar_queue_consumer_info_t ar_queue_get_consumer_info(const ar_queue_t* queue) {
  return g_cq_mock->CircularQueueGetConsumerInfo(queue);
}

/**
 * Get the ar queue producer info.
 *
 * @param[in] queue The ar_queue
 *
 * @retval Producer info
 */
ar_queue_producer_info_t ar_queue_get_producer_info(const ar_queue_t* queue) {
  return g_cq_mock->CircularQueueGetProducerInfo(queue);
}
