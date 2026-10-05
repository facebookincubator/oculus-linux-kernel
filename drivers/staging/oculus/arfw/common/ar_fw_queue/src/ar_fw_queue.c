#include "ar_fw_queue.h"
#include "ar_queue_consumer.h"

#define ROUNDUP(a, b) ((((a) + ((b)-1)) / (b)) * (b))

int ar_fw_queue_meta_init(
    uint16_t depth,
    uint32_t element_size,
    queue_direction_t queue_direction,
    uint32_t dcache_line_size,
    uint32_t page_size,
    ar_fw_queue_meta_t* meta) {
  size_t consume_size, produce_size, data_size;
  ar_status_t status;

  /*
   * Data may be shared with DMA hardware. Enforce cache alignment such that the
   * drive can manage flush+invalidate.
   *
   * So long as the queue is padded from other data out to cache line, it would
   * be possible to skip this check. However, that would result in buffers
   * within the queue sharing a cache line. It would function, but the
   * flush+invalidate on send/receive would impact neighboring buffers, likely
   * resulting in a stall while cache is refreshed. This could happen randomly
   * during potentially critical code. Better if avoided.
   */
  if (element_size % dcache_line_size != 0) {
    return -AR_ERR_INVALID_ARGS;
  }

  if (element_size > page_size) {
    return -AR_ERR_INVALID_ARGS;
  }

  status = ar_queue_required_size(
      element_size, depth, page_size, &produce_size, &consume_size, &data_size);
  if (status != AR_OK) {
    return status;
  }

  // we asked for page size aligned sizes, assert that here.
  AR_ASSERT((produce_size % page_size) == 0);
  AR_ASSERT((consume_size % page_size) == 0);

  meta->depth = depth;
  meta->queue_direction = queue_direction;
  meta->element_size = element_size;
  meta->dcache_line_size = dcache_line_size;
  meta->page_size = page_size;

  meta->produce_meta_size = produce_size;
  meta->consume_meta_size = consume_size;
  meta->data_size = data_size;
  meta->total_mem_size =
      produce_size + consume_size + page_size + data_size; // queue and guard page

  return 0;
}
EXPORT_SYMBOL(ar_fw_queue_meta_init);

int ar_fw_queue_slot_reserve(ar_fw_queue_t* queue, ar_fw_io_request_t* io_request) {
  ar_packet_t* entry;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(io_request != NULL);

  io_request->data_size = queue->submit_element_size;

  entry = ar_queue_get_next_produce(&queue->submit_queue, &io_request->index);
  if (entry == NULL) {
    return -AR_ERR_NO_MEMORY;
  }

  io_request->data = (void*)entry;

  return 0;
}
EXPORT_SYMBOL(ar_fw_queue_slot_reserve);

bool ar_fw_queue_slot_produce(ar_fw_queue_t* queue, ar_fw_io_request_t* io_request) {
  AR_ASSERT(queue != NULL);
  AR_ASSERT(io_request != NULL);

  return ar_queue_mark_element_consume_ready(&queue->submit_queue, io_request->index) == AR_OK;
}
EXPORT_SYMBOL(ar_fw_queue_slot_produce);

/// Helper to ensure that the params and meta combination are sensible
static int validate_creator_data(
    const ar_fw_queue_meta_t* meta,
    const ar_fw_queue_creator_data_t* params) {
  // see explanation in ar_fw_queue_create of why this works.
  bool receiver = !((params->queue_creator == DRIVER) ^ (meta->queue_direction == SEND));

  if (receiver && params->request_callback == NULL) {
    return -AR_ERR_INVALID_ARGS;
  }
  return 0;
}

/// Internal helper to create a circular queue.
static int queue_create(
    uintptr_t meta_base,
    size_t produce_size,
    size_t consume_size,
    uintptr_t data_base,
    size_t data_size,
    bool producer,
    uint16_t depth,
    uint32_t element_size,
    uint32_t page_size,
    ar_queue_t* circular_queue) {
  ar_queue_setup_t queue_setup;

  queue_setup = (ar_queue_setup_t){
      .owner = producer ? AR_QUEUE_PRODUCER : AR_QUEUE_CONSUMER,
      .element_size = element_size,
      .element_count = depth,
      .align = page_size,
  };

  queue_setup.produce_meta = (void*)meta_base;
  queue_setup.produce_size = produce_size;
  queue_setup.consume_meta = (void*)(meta_base + produce_size);
  queue_setup.consume_size = consume_size;
  queue_setup.data = (void*)data_base;
  queue_setup.data_size = data_size;
  return ar_queue_create(&queue_setup, circular_queue);
}

int ar_fw_queue_create(
    const ar_fw_queue_meta_t* meta,
    uintptr_t meta_mem_base,
    uint64_t meta_mem_size,
    uintptr_t data_mem_base,
    uint64_t data_mem_size,
    const ar_fw_queue_creator_data_t* params,
    ar_fw_queue_t* result) {
  int error;
  bool is_produce;
  ar_fw_queue_meta_t meta_copy;

  if (meta == NULL || params == NULL || result == NULL) {
    return -AR_ERR_INVALID_ARGS;
  }

  // validate all queue parameters
  error = ar_fw_queue_meta_init(
      meta->depth,
      meta->element_size,
      meta->queue_direction,
      meta->dcache_line_size,
      meta->page_size,
      &meta_copy);
  if (error) {
    return error;
  }

  if (meta->total_mem_size != meta_copy.total_mem_size || meta->depth != meta_copy.depth ||
      meta_mem_size < meta_copy.produce_meta_size + meta_copy.consume_meta_size ||
      data_mem_size < meta_copy.data_size ||
      meta->produce_meta_size != meta_copy.produce_meta_size ||
      meta->consume_meta_size != meta_copy.consume_meta_size ||
      meta->data_size != meta_copy.data_size) {
    return -AR_ERR_INVALID_ARGS;
  }

  error = validate_creator_data(&meta_copy, params);
  if (error) {
    return error;
  }

  memset(result, 0, sizeof(ar_fw_queue_t));

  /**
   * The producer is the one placing data into the queue, although
   * the client always owns the memory. This is an xor.
   * QUEUE CREATOR | QUEUE TYPE | IS PRODUCE
   * DRIVER        | SEND       | NO
   * DRIVER        | RECEIVE    | YES
   * CLIENT        | SEND       | YES
   * CLIENT        | RECEIVE    | NO
   */
  is_produce = (params->queue_creator == DRIVER) ^ (meta->queue_direction == SEND);

  error = queue_create(
      meta_mem_base,
      meta_copy.produce_meta_size,
      meta_copy.consume_meta_size,
      data_mem_base,
      data_mem_size,
      is_produce,
      meta_copy.depth,
      meta_copy.element_size,
      meta_copy.page_size,
      &result->submit_queue);
  if (error) {
    return error;
  }

  result->size = meta_copy.depth;
  result->submit_element_size = meta_copy.element_size;
  result->params = *params;

  return 0;
}
EXPORT_SYMBOL(ar_fw_queue_create);

int ar_fw_queue_destroy(ar_fw_queue_t* queue) {
  AR_ASSERT(queue != NULL);
  ar_queue_destroy(&queue->submit_queue);
  return 0;
}
EXPORT_SYMBOL(ar_fw_queue_destroy);

bool ar_fw_queue_mark_slot_ready(ar_fw_queue_t* queue, uint32_t index) {
  return ar_queue_mark_element_produce_ready(&queue->submit_queue, index) == AR_OK;
}
EXPORT_SYMBOL(ar_fw_queue_mark_slot_ready);

size_t ar_fw_queue_poll(ar_fw_queue_t* queue, size_t count, void* caller_request) {
  ar_packet_t* data = NULL;
  uint32_t consume_index = 0;
  ar_queue_t* submit_queue = &queue->submit_queue;
  size_t total = 0;
  bool mark_consumed;
  uint16_t consumable_count;

  /*
   * The loop below processes all available requests one by one, while doing
   * so it marks the element as produced ready locally, but don't send any signal
   * that messages are consumed to another peer. This could result in some weird
   * unexpected behavior:
   *   - user callback inside the request_callback call sends back reply
   *   - other peer gets reply and sends new message
   *   - if such behavior continues, then soon the queue depth would be reached on
   *     other side
   * Limit the amount of requests to serve, by checking the number of requests
   * available.
   * Note: there is still a possibility that other side produces faster, then it is
   * consumed on this side. But this is a usual back pressure case and should not be
   * handled differently.
   */
  consumable_count = ar_fw_queue_get_consumable_count(queue);
  if (count > consumable_count) {
    count = consumable_count;
  }
  while (total < count) {
    bool success;
    ar_fw_io_request_t io_request;

    /// Pull the data pointer from the queue
    data = ar_queue_get_next_consume(submit_queue, &consume_index);
    if (data == NULL) {
      return total;
    }

    io_request.data = data;
    io_request.data_size = queue->submit_element_size;
    io_request.index = consume_index;

    /// Dispatch the callback
    success = queue->params.request_callback(
        queue->params.callback_context, &io_request, caller_request, &mark_consumed);

    if (mark_consumed) {
      /// Mark the element as ready
      ar_queue_mark_element_produce_ready(submit_queue, consume_index);
    }

    /*
     * Do not attempt to continue if the callback failed, although the item is
     * consumed. This gives the caller an opportunity to stop polling if
     * necessary
     */
    if (!success) {
      break;
    }
    total++;
  }

  return total;
}
EXPORT_SYMBOL(ar_fw_queue_poll);

bool ar_fw_queue_set_callback(
    ar_fw_queue_t* queue,
    ar_fw_request_callback_t request_callback_override,
    void* context_override) {
  if (request_callback_override == NULL) {
    return false;
  }
  queue->params.request_callback = request_callback_override;
  queue->params.callback_context = context_override;
  return true;
}
EXPORT_SYMBOL(ar_fw_queue_set_callback);

uintptr_t ar_fw_queue_base_address_get(ar_fw_queue_t* queue) {
  return (uintptr_t)queue->submit_queue.data;
}
EXPORT_SYMBOL(ar_fw_queue_base_address_get);

uint16_t ar_fw_queue_get_consumable_count(const ar_fw_queue_t* queue) {
  ar_queue_producer_info_t p_info;
  ar_queue_consumer_info_t c_info;

  AR_ASSERT(queue != NULL);

  p_info = ar_queue_get_producer_info(&queue->submit_queue);
  c_info = ar_queue_get_consumer_info(&queue->submit_queue);

  if (p_info.iteration != c_info.iteration) {
    return queue->size + p_info.index - c_info.index - p_info.in_flight;
  }

  return p_info.index - c_info.index - p_info.in_flight;
}
EXPORT_SYMBOL(ar_fw_queue_get_consumable_count);
