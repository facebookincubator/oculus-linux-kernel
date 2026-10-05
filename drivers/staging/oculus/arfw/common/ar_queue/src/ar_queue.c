#include "ar_queue_consumer.h"
#include "ar_queue_int.h"

#include <ar_atomics.h>
#include <ar_utils.h>

#ifdef __KERNEL__
#include <linux/module.h>

MODULE_LICENSE("Dual BSD/GPL");

// macro from asm/current.h causes compilation errors
#undef current
#endif /* __KERNEL__ */

ar_status_t ar_queue_create(ar_queue_setup_t* queue_setup, ar_queue_t* ar_queue) {
  size_t calculated_produce_size, calculated_consume_size;

  uint64_t* ownership = NULL;
  uint32_t num_ownership;

  const size_t align_max = AR_KB * 4;
  size_t align;
  uint32_t element_size;
  uint16_t element_count;
  ar_queue_produce_t* produce;
  ar_queue_consume_t* consume;

  uint32_t i;

  if (queue_setup == NULL || ar_queue == NULL) {
    return -AR_ERR_INVALID_ARGS;
  }

  if (queue_setup->produce_meta == NULL || queue_setup->produce_size == 0 ||
      queue_setup->consume_meta == NULL || queue_setup->consume_size == 0 ||
      queue_setup->data == NULL || queue_setup->data_size == 0 || queue_setup->element_size == 0 ||
      queue_setup->element_count == 0 || queue_setup->element_size > UINT32_MAX ||
      AR_QUEUE_MAX_ELEMENTS < queue_setup->element_count) {
    return -AR_ERR_INVALID_ARGS;
  }

  align = queue_setup->align;
  element_size = (uint32_t)queue_setup->element_size;
  element_count = queue_setup->element_count;
  produce = queue_setup->produce_meta;
  consume = queue_setup->consume_meta;

  // data portion of queue must hold all the elements (no metadata required)
  if (queue_setup->data_size < element_size * element_count) {
    return -AR_ERR_INVALID_ARGS;
  }

  // Alignment must be a positive multiple of AR_QUEUE_ALIGN_DEFAULT
  if (align == 0 || align_max < align || align % AR_QUEUE_ALIGN_DEFAULT != 0) {
    return -AR_ERR_INVALID_ARGS;
  }

  calculated_produce_size = sizeof(*produce) + element_count * sizeof(ar_queue_packet_meta_t);

  AR_ASSERT(sizeof(*ownership) == sizeof(void*));
  num_ownership = AR_COUNT_TO_MASKCOUNT(element_count, AR_OWNERSHIP_BITS);
  calculated_consume_size = sizeof(*consume) + num_ownership * sizeof(*ownership);

  if (AR_ROUNDUP(calculated_consume_size, queue_setup->align) != queue_setup->consume_size ||
      AR_ROUNDUP(calculated_produce_size, queue_setup->align) != queue_setup->produce_size) {
    return -AR_ERR_INVALID_ARGS;
  }

  /*
   * Data region must match the requested alignment.
   * Consume and produce region must be 8-byte aligned so that uint64 atomic
   * operations will work.
   */
  if (((uintptr_t)queue_setup->data % align) != 0 || ((uintptr_t)consume % sizeof(uint64_t)) != 0 ||
      ((uintptr_t)produce % sizeof(uint64_t)) != 0) {
    return -AR_ERR_BAD_ALIGNMENT;
  }

  if (queue_setup->owner == AR_QUEUE_PRODUCER) {
    ar_atomic_store(&produce->element_count, element_count, AR_MEMORY_ORDER_SEQ_CST);
    produce->element_size = element_size;
    ar_atomic64_store(&produce->produced_counter.atomic, 0, AR_MEMORY_ORDER_RELAXED);
    ar_atomic64_store(&consume->consumed_counter.atomic, 0, AR_MEMORY_ORDER_RELAXED);

    ownership = (void*)consume->ownership;
    memset(ownership, AR_QUEUE_PRODUCER, num_ownership * sizeof(*ownership));

    for (i = 0; i < element_count; ++i) {
      ar_queue_packet_meta_t* const packet_meta = ar_queue_get_packet_meta(produce->metadata, i);
      ar_atomic_store(&packet_meta->iteration, AR_QUEUE_PACKET_UNUSED, AR_MEMORY_ORDER_RELAXED);
    }
  } else { // AR_QUEUE_CONSUMER
    ar_atomic_store(&consume->element_count, element_count, AR_MEMORY_ORDER_RELAXED);
    consume->element_size = element_size;
  }

  ar_queue->produce = produce;
  ar_queue->consume = consume;
  ar_queue->data = queue_setup->data;
  ar_queue->owner = queue_setup->owner;
  ar_queue->element_count = element_count;

  return AR_OK;
}
EXPORT_SYMBOL(ar_queue_create);

bool ar_queue_is_empty(ar_queue_t* queue) {
  ar_queue_produced_t produced;
  ar_queue_consumed_t consumed;
  uint16_t produced_index, consumed_index;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);
  consumed.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  produced_index = produced.generic.index;
  consumed_index = consumed.consumed.index;

  if (produced_index != consumed_index) {
    return false;
  }

  return ar_queue_check_ownership(produced_index, ar_queue_get_ownership(queue), AR_QUEUE_PRODUCER);
}
EXPORT_SYMBOL(ar_queue_is_empty);

void ar_queue_destroy(ar_queue_t* queue) {
  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  queue->produce = NULL;
  queue->consume = NULL;
}
EXPORT_SYMBOL(ar_queue_destroy);

ar_status_t ar_queue_required_size(
    size_t element_size,
    uint16_t element_count,
    size_t align,
    size_t* produce_meta_size,
    size_t* consume_meta_size,
    size_t* data_size) {
  size_t align_max, produce_size_computed, consume_size_computed, data_size_computed;
  uint32_t num_ownership;

  AR_ASSERT(produce_meta_size != NULL);
  AR_ASSERT(consume_meta_size != NULL);
  AR_ASSERT(data_size != NULL);

  if (element_size == 0 || UINT32_MAX < element_size || element_count == 0) {
    return -AR_ERR_INVALID_ARGS;
  }

  // Make sure align is within reasonable range.
  align_max = AR_KB * 4;
  if (align == 0 || align_max < align || align % AR_QUEUE_ALIGN_DEFAULT != 0) {
    return -AR_ERR_INVALID_ARGS;
  }

  // No overflow possible in calculating produce_size.
  produce_size_computed =
      sizeof(ar_queue_produce_t) + element_count * sizeof(ar_queue_packet_meta_t);

  // data size is simply elem count x elem size
  data_size_computed = element_count * element_size;

  // No overflow possible in calculating consume_size as well.
  num_ownership = AR_COUNT_TO_MASKCOUNT(element_count, AR_OWNERSHIP_BITS);
  consume_size_computed = sizeof(ar_queue_consume_t) + num_ownership * sizeof(uint64_t);

  /*
   * Round up the size here to align
   */
  *produce_meta_size = AR_ROUNDUP(produce_size_computed, align);
  *consume_meta_size = AR_ROUNDUP(consume_size_computed, align);
  *data_size = AR_ROUNDUP(data_size_computed, align);

  return AR_OK;
}
EXPORT_SYMBOL(ar_queue_required_size);

ar_packet_t* ar_queue_get_next_produce(ar_queue_t* queue, uint32_t* element_idx) {
  ar_queue_produced_t prev_current;
  ar_queue_produced_t current;
  ar_queue_produced_t next;
  ar_queue_consumed_t consumer_current;
  void* packet;
  ar_queue_packet_meta_t* packet_meta;
  uint16_t cur_packet_iteration;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(element_idx != NULL);
  AR_ASSERT(queue->owner == AR_QUEUE_PRODUCER);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  current.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  do {
    bool can_produce;
    int element_count = ar_atomic_load(&queue->produce->element_count, AR_MEMORY_ORDER_SEQ_CST);

    // Check whether there is still space in the circular queue.
    if (AR_UNLIKELY(current.generic.in_flight == element_count)) {
      return NULL;
    }

    // Perform the check for the current position.
    packet_meta = ar_queue_get_packet_meta(queue->produce->metadata, current.generic.index);
    cur_packet_iteration = ar_atomic_load(&packet_meta->iteration, AR_MEMORY_ORDER_ACQUIRE);
    consumer_current.value =
        ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

    can_produce = ar_queue_can_produce_idx(
        queue,
        current.generic.iteration,
        current.generic.index,
        cur_packet_iteration,
        consumer_current.consumed.iteration);

    if (!can_produce) {
      goto recheck;
    }
    next.value = current.value;

    /*
     * If we are going to produce a new item into the queue, we need to advance
     * the next index, and mark that we have an operation in-flight.
     */
    next.generic.index = (current.generic.index + 1) % element_count;
    if (next.generic.index == 0) {
      next.generic.iteration = (next.generic.iteration + 1) % AR_QUEUE_ITERATION_COUNT;
    }
    next.generic.in_flight++;
    AR_ASSERT(next.generic.in_flight > 0);

    /*
     * The atomic instruction updates the current value, so we need to store
     * the previous value prior to performing the compare exchange operation.
     */
    prev_current.value = current.value;
    if (ar_atomic64_compare_exchange(
            &queue->produce->produced_counter.atomic,
            &current.value,
            next.value,
            AR_MEMORY_ORDER_SEQ_CST,
            AR_MEMORY_ORDER_SEQ_CST)) {
      *element_idx = current.generic.index;
      goto success;
    } else {
      continue;
    }
  recheck:
    prev_current.value = current.value;
    current.value =
        ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);
  } while (prev_current.value != current.value);

  // the indexes did not change, no packets available
  return NULL;

success:
  packet = ar_queue_get_packet(queue, *element_idx);
  return packet;
}
EXPORT_SYMBOL(ar_queue_get_next_produce);

void* ar_queue_get_next_consume(ar_queue_t* queue, uint32_t* element_idx) {
  ar_queue_consumed_t current;
  ar_queue_consumed_t next;
  void* packet;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(element_idx != NULL);
  AR_ASSERT(queue->owner == AR_QUEUE_CONSUMER);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  do {
    ar_queue_packet_meta_t* packet_meta;

    current.value =
        ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

    next.value = ar_queue_calculate_next_consumed_counter(queue);

    if (queue->owner == AR_QUEUE_PRODUCER) {
      /*
       * When produce tries to drain the channel, make sure that the consume
       * counter has not been corrupted by other consume attacker.
       */
      if (current.consumed.index >=
          ar_atomic_load(&queue->produce->element_count, AR_MEMORY_ORDER_SEQ_CST)) {
        return NULL;
      }
    }

    packet_meta = ar_queue_get_packet_meta(queue->produce->metadata, current.consumed.index);

    if (!ar_check_consume_packet_iteration(current, packet_meta)) {
      return NULL;
    }

    if (ar_atomic64_compare_exchange(
            &queue->consume->consumed_counter.atomic,
            (long*)&current,
            next.value,
            AR_MEMORY_ORDER_SEQ_CST,
            AR_MEMORY_ORDER_SEQ_CST)) {
      *element_idx = current.consumed.index;
      break;
    }
  } while (true);

  packet = ar_queue_get_packet(queue, *element_idx);

  return packet;
}
EXPORT_SYMBOL(ar_queue_get_next_consume);

ar_status_t ar_queue_mark_element_consume_ready(ar_queue_t* queue, uint32_t element_idx) {
  ar_queue_produce_t* produce;
  ar_queue_produced_t current;
  ar_queue_produced_t next;
  ar_queue_packet_meta_t* packet_meta;
  ar_status_t status;
  uint16_t element_count = ar_atomic_load(&queue->produce->element_count, AR_MEMORY_ORDER_SEQ_CST);

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->owner == AR_QUEUE_PRODUCER);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  if (element_idx >= element_count)
    return -AR_ERR_INVALID_ARGS;

  produce = (ar_queue_produce_t*)queue->produce;

  do {
    current.value =
        ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

    AR_ASSERT(current.generic.in_flight > 0);

    next.value = current.value;
    next.generic.in_flight--;

    if (ar_atomic64_compare_exchange(
            &queue->produce->produced_counter.atomic,
            (long*)&current,
            next.value,
            AR_MEMORY_ORDER_SEQ_CST,
            AR_MEMORY_ORDER_SEQ_CST)) {
      break;
    }

  } while (true);

  // clang-static-analysis-disable-next-line
  status = ar_queue_flip_ownership(element_idx, ar_queue_get_ownership(queue), queue->owner);

  if (status != AR_OK)
    return status;

  /*
   * We need to ensure that the update the ownership array happens before
   * we update the packet iteration. However, a memory barrier is not
   * necessary because the ownership update and the interation update are two
   * sequentially consistent atomic operations.
   *
   * If the two operations happened in the reverse order, the slot could
   * appear as free to another producer between the two updates (the
   * iteration would be one behind the producer iteration and the producer
   * would own the slot). Should that happen, the slot might be bogusly
   * claimed.
   *
   * See the checks in circular_queue_generic_can_produce_idx for details.
   */

  // Increments the iteration field in the packet to finalize the packet.
  packet_meta = ar_queue_get_packet_meta(produce->metadata, element_idx);
  ar_increment_packet_iteration(packet_meta);

  return AR_OK;
}
EXPORT_SYMBOL(ar_queue_mark_element_consume_ready);

ar_status_t ar_queue_mark_element_produce_ready(ar_queue_t* queue, int element_idx) {
  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->owner == AR_QUEUE_CONSUMER);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  if (element_idx >= ar_atomic_load(&queue->consume->element_count, AR_MEMORY_ORDER_RELAXED))
    return -AR_ERR_INVALID_ARGS;

  // clang-static-analysis-disable-next-line
  return ar_queue_flip_ownership(element_idx, ar_queue_get_ownership(queue), queue->owner);
}
EXPORT_SYMBOL(ar_queue_mark_element_produce_ready);

long ar_queue_calculate_next_consumed_counter(ar_queue_t* queue) {
  int consume_element_count;
  ar_queue_consumed_t current;
  ar_queue_consumed_t next;

  consume_element_count = ar_atomic_load(&queue->consume->element_count, AR_MEMORY_ORDER_SEQ_CST);

  current.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);
  next.value = current.value;
  next.consumed.index = (current.consumed.index + 1) % consume_element_count;

  // Check whether there is a consume index wrap up.
  if (next.consumed.index != 0) {
    return next.value;
  }

  AR_ASSERT(consume_element_count == queue->element_count);

  // wrap up and increase iteration.
  next.consumed.iteration = (next.consumed.iteration + 1) % AR_QUEUE_ITERATION_COUNT;

  return next.value;
}
EXPORT_SYMBOL(ar_queue_calculate_next_consumed_counter);

bool ar_queue_packet_available(ar_queue_t* queue) {
  ar_queue_consumed_t current;
  ar_queue_packet_meta_t* packet_meta;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);
  AR_ASSERT(queue->consume != NULL);

  current.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  packet_meta = ar_queue_get_packet_meta(queue->produce->metadata, current.consumed.index);

  /*
   * Not all implementations of the circular queue use the ownership array,
   * in addition, we know that the current index points at a not-yet read
   * packet. If the packet iteration matches the iteration of the consumer
   * queue, then the packet got fully produced and is available.
   * Since the write to the iteration happens as the last step of finializing
   * the packet, we must use an atomic load instruction to guarantee that this
   * thread is going to see the write that might have happened on another
   * thread.
   */
  return ar_check_consume_packet_iteration(current, packet_meta);
}
EXPORT_SYMBOL(ar_queue_packet_available);

static int get_ownership(int idx, ar_queue_ownership_t* ownership) {
  uint32_t ownership_idx = idx / AR_OWNERSHIP_BITS;
  uint32_t ownership_bit_position = idx % AR_OWNERSHIP_BITS;
  uint64_t ownership_offset_mask = (uint64_t)1 << ownership_bit_position;
  uint64_t ownership_result =
      ar_atomic64_load(&ownership[ownership_idx].atomic, AR_MEMORY_ORDER_SEQ_CST);

  return (ownership_result & ownership_offset_mask);
}

#define AR_OWNER_DUMP_WIDTH 32

int ar_queue_dump(ar_queue_t* queue, char* buf, size_t buflen) {
  int ret, temp, col_count;
  ar_queue_produced_t produced;
  ar_queue_consumed_t consumed;
  ar_queue_ownership_t* ownership;

  // write out overall queue metadata
  ret = snprintf(
      buf,
      buflen,
      "queue: %p. produce: %p, consume: %p, data: %p, type: %s, depth: %u\n",
      queue,
      queue->produce,
      queue->consume,
      queue->data,
      (queue->owner == AR_QUEUE_CONSUMER ? "consumer" : "producer"),
      queue->element_count);

  if (ret < 0 || (size_t)ret == buflen)
    return -1;

  buf += ret;
  buflen -= ret;

  // dump queue->consume data
  consumed.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);
  ret = snprintf(
      buf,
      buflen,
      "queue consume: element count %d, index %u, iteration %u\n",
      ar_atomic_load(&queue->consume->element_count, AR_MEMORY_ORDER_SEQ_CST),
      consumed.consumed.index,
      consumed.consumed.iteration);

  if (ret < 0 || (size_t)ret == buflen)
    return -1;

  buf += ret;
  buflen -= ret;

  // dump queue->produce data
  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);
  ret = snprintf(
      buf,
      buflen,
      "queue produce: element count %d, index %u, iteration %u, in flight %u\n",
      ar_atomic_load(&queue->produce->element_count, AR_MEMORY_ORDER_SEQ_CST),
      produced.generic.index,
      produced.generic.iteration,
      produced.generic.in_flight);

  if (ret < 0 || (size_t)ret == buflen)
    return -AR_ERR_INVALID_ARGS;

  buf += ret;
  buflen -= ret;

  // dump ownership array
  ret = snprintf(buf, buflen, "ownership:\n");
  if (ret < 0 || (size_t)ret == buflen)
    return -AR_ERR_INVALID_ARGS;

  buf += ret;
  buflen -= ret;

  col_count =
      AR_OWNER_DUMP_WIDTH < queue->element_count ? AR_OWNER_DUMP_WIDTH : queue->element_count;
  for (temp = 0; temp < col_count; temp++) {
    ret = snprintf(buf, buflen, "%02d ", temp);

    if (ret < 0 || (size_t)ret == buflen)
      return -AR_ERR_INVALID_ARGS;

    buf += ret;
    buflen -= ret;
  }

  ownership = ar_queue_get_ownership(queue);
  for (temp = 0; temp < queue->element_count; temp++) {
    ret = snprintf(
        buf,
        buflen,
        "%s%c |",
        (temp % col_count == 0 ? "\n|" : ""),
        (get_ownership(temp, ownership) == AR_QUEUE_PRODUCER ? 'p' : 'c'));

    if (ret < 0 || (size_t)ret == buflen)
      return -AR_ERR_INVALID_ARGS;

    buf += ret;
    buflen -= ret;
  }

  return 0;
}
EXPORT_SYMBOL(ar_queue_dump);

ar_queue_consumer_info_t ar_queue_get_consumer_info(const ar_queue_t* queue) {
  ar_queue_consumed_t consumed;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->consume != NULL);

  consumed.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return consumed.consumed;
}
EXPORT_SYMBOL(ar_queue_get_consumer_info);

ar_queue_producer_info_t ar_queue_get_producer_info(const ar_queue_t* queue) {
  ar_queue_produced_t produced;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);

  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return produced.generic;
}
EXPORT_SYMBOL(ar_queue_get_producer_info);

uint16_t ar_queue_get_consumer_idx(ar_queue_t* queue) {
  ar_queue_consumed_t consumed;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->consume != NULL);

  consumed.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return consumed.consumed.index;
}
EXPORT_SYMBOL(ar_queue_get_consumer_idx);

uint16_t ar_queue_get_producer_idx(ar_queue_t* queue) {
  ar_queue_produced_t produced;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);

  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return produced.generic.index;
}
EXPORT_SYMBOL(ar_queue_get_producer_idx);

uint16_t ar_queue_get_producer_inflight(ar_queue_t* queue) {
  ar_queue_produced_t produced;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);

  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return produced.generic.in_flight;
}
EXPORT_SYMBOL(ar_queue_get_producer_inflight);

uint16_t ar_queue_get_consumer_iteration(ar_queue_t* queue) {
  ar_queue_consumed_t consumed;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->consume != NULL);

  consumed.value =
      ar_atomic64_load(&queue->consume->consumed_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return consumed.consumed.iteration;
}
EXPORT_SYMBOL(ar_queue_get_consumer_iteration);

uint16_t ar_queue_get_producer_iteration(ar_queue_t* queue) {
  ar_queue_produced_t produced;

  AR_ASSERT(queue != NULL);
  AR_ASSERT(queue->produce != NULL);

  produced.value =
      ar_atomic64_load(&queue->produce->produced_counter.atomic, AR_MEMORY_ORDER_SEQ_CST);

  return produced.generic.iteration;
}
EXPORT_SYMBOL(ar_queue_get_producer_iteration);
