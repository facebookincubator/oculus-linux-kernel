#include "ar_queue_int.h"

#include <ar_utils.h>
#include "ar_atomics.h"

bool ar_queue_check_ownership(
    uint32_t idx,
    ar_queue_ownership_t* ownership,
    ar_queue_owner_t desired) {
  uint32_t ownership_idx = idx / AR_OWNERSHIP_BITS;
  uint32_t ownership_bit_position = idx % AR_OWNERSHIP_BITS;
  uint64_t ownership_offset_mask = (uint64_t)1 << ownership_bit_position;
  uint64_t ownership_desired_result = (uint64_t)desired << ownership_bit_position;
  uint64_t ownership_result =
      ar_atomic64_load(&ownership[ownership_idx].atomic, AR_MEMORY_ORDER_SEQ_CST);

  return (ownership_result & ownership_offset_mask) == ownership_desired_result;
}

ar_status_t ar_queue_flip_ownership(
    uint32_t element_idx,
    ar_queue_ownership_t* ownership,
    ar_queue_owner_t owner) {
  uint32_t ownership_idx = element_idx / AR_OWNERSHIP_BITS;
  uint32_t ownership_bit_pos = element_idx % AR_OWNERSHIP_BITS;
  uint64_t ownership_offset_mask = (uint64_t)1 << ownership_bit_pos;
  uint64_t ownership_result = (uint64_t)owner << ownership_bit_pos;

  ar_queue_ownership_t old_ownership;
  ar_queue_ownership_t new_ownership;

  do {
    old_ownership.value =
        ar_atomic64_load(&ownership[ownership_idx].atomic, AR_MEMORY_ORDER_SEQ_CST);
    if ((old_ownership.value & ownership_offset_mask) != ownership_result) {
      return AR_ERR_BAD_STATE;
    }

    new_ownership.value = old_ownership.value ^ ownership_offset_mask;
    if (ar_atomic64_compare_exchange(
            &ownership[ownership_idx].atomic,
            &old_ownership.value,
            new_ownership.value,
            AR_MEMORY_ORDER_SEQ_CST,
            AR_MEMORY_ORDER_SEQ_CST)) {
      break;
    }
  } while (true);

  return AR_OK;
}

void ar_increment_packet_iteration(ar_queue_packet_meta_t* packet_meta) {
  /*
   * We have to use the sequential add instruction, since we need to
   * make sure that the write is visible to concurrent reads, and
   * the write must not be issued prior to executing any other
   * instruction, since it finalizes a packet.
   */
  int old_iteration = ar_atomic_load(&packet_meta->iteration, AR_MEMORY_ORDER_SEQ_CST);

  /*
   * UINT16_MAX (AR_QUEUE_PACKET_UNUSED) is reserved
   * for unused packet iteration.
   */
  int new_val =
      old_iteration == AR_QUEUE_PACKET_UNUSED ? 0 : (old_iteration + 1) % AR_QUEUE_ITERATION_COUNT;
  int ret_old_iteration = old_iteration;

  bool success = ar_atomic_compare_exchange(
      &packet_meta->iteration,
      &ret_old_iteration,
      new_val,
      AR_MEMORY_ORDER_SEQ_CST,
      AR_MEMORY_ORDER_SEQ_CST);

  AR_ASSERT(success);
}

bool ar_queue_can_produce_idx(
    ar_queue_t* queue,
    uint16_t queue_iteration,
    uint32_t packet_index,
    uint16_t packet_iteration,
    uint16_t consumer_iteration) {
  /*
   * To be able to claim the packet index the following conditions must
   * be met:
   *
   * a). The packet has been created on expansion request and it has not
   *     not been used (packet_iteration == AR_QUEUE_PACKET_UNUSED).
   *
   * or
   *
   * b1.) The packet iteration must be one iteration behind
   *     the queue iteration. If the packet iteration is two
   *     generations behind, it means that the packet is still getting
   *     produced by another thread, and we cannot produce new items until
   *     that item got fully produced. If the packet iteration equals the
   *     queue iteration, this thread has lost the race for this index, and
   *     another thread already claimed this index.
   *
   * b2.) The owner of the index must be AR_QUEUE_PRODUCER.
   *
   * We must check the conditions in this sequence, since
   * ar_queue_mark_element_consume_ready will update the owner of
   * index at first, and then the packet iteration.
   *
   * If we would check the ownership first, it can lead to the situation
   * where the ownership is good because another PRODUCER thread hasn't marked
   * the packet as CONSUME_READY, but after we checked the ownership the other
   * PRODUCER updates the packet iteration, at which point the checks for this
   * thread would pass, and we would have given out the same index multiple
   * times.
   *
   */

  // Check a.)
  if (packet_iteration == AR_QUEUE_PACKET_UNUSED && queue_iteration == consumer_iteration) {
    return true;
  }

  // Check b1.)
  if (!ar_check_produce_iteration(queue_iteration, packet_iteration)) {
    return false;
  }

  /* We need to guarantee the packet key/iteration is read before the
   * ownership bit. The SMP read barrier will guarantee that. From ARM
   * infocenter: DMB() ISHLD DMB() operation that waits only for loads to
   * complete, and only applies to the inner shareable domain (processors).
   * See comment above for details.
   */
  ar_smp_read_mb();

  // Check b2.)
  if (!ar_queue_check_ownership(packet_index, ar_queue_get_ownership(queue), AR_QUEUE_PRODUCER)) {
    return false;
  }
  return true;
}

bool ar_check_consume_packet_iteration(
    ar_queue_consumed_t required_consume,
    const ar_queue_packet_meta_t* packet_meta) {
  return required_consume.consumed.iteration ==
      ar_atomic_load(&packet_meta->iteration, AR_MEMORY_ORDER_ACQUIRE) &&
      required_consume.consumed.iteration != AR_QUEUE_PACKET_UNUSED;
}
