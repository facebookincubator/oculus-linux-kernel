// Copyright (c) Meta Technologies, LLC and its affiliates. All Rights reserved.

#include <FrlGtestWrapper.h>
#include <gtest/gtest.h>

#include <ar_queue_consumer.h>

typedef uint64_t packet_t;
static const packet_t c_data_prefix = 0xbaadbaad00000000;

static const int c_queue_depth = 32;

static ar_queue_t ar_queue_produce, ar_queue_consume;
static bool ar_queues_created;
static void* produce_ptr = nullptr;
static void* consume_ptr = nullptr;
static void* data_ptr = nullptr;
static size_t produce_size = 0;
static size_t consume_size = 0;
static size_t data_size = 0;

class ArQueueTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {
    if (ar_queues_created) {
      ar_queue_destroy(&ar_queue_produce);
      ar_queue_destroy(&ar_queue_consume);
    }
    if (produce_ptr != nullptr) {
      free(produce_ptr);
      produce_ptr = nullptr;
    }
    if (consume_ptr != nullptr) {
      free(consume_ptr);
      consume_ptr = nullptr;
    }
    if (data_ptr != nullptr) {
      free(data_ptr);
      data_ptr = nullptr;
    }
  }
};

/**
 * Initializes queue_setup to be a valid producer, setting
 * produce_ptr/consume_ptr/produce_size/consume_size as a side effect.
 */
static void create_valid_setup(ar_queue_setup_t* queue_setup, int queue_depth) {
  auto res = ar_queue_required_size(
      sizeof(packet_t),
      queue_depth,
      AR_QUEUE_ALIGN_DEFAULT,
      &produce_size,
      &consume_size,
      &data_size);
  EXPECT_TRUE(res == AR_OK);
  produce_ptr = malloc(produce_size);
  consume_ptr = malloc(consume_size);
  data_ptr = malloc(data_size);

  queue_setup->produce_meta = produce_ptr;
  queue_setup->produce_size = produce_size;
  queue_setup->consume_meta = consume_ptr;
  queue_setup->consume_size = consume_size;
  queue_setup->data = data_ptr;
  queue_setup->data_size = data_size;
  queue_setup->owner = AR_QUEUE_PRODUCER;
  queue_setup->element_size = sizeof(packet_t);
  queue_setup->element_count = queue_depth;
  queue_setup->align = AR_QUEUE_ALIGN_DEFAULT;
}

static void create_queues(ar_queue_setup_t* queue_setup, int queue_depth) {
  create_valid_setup(queue_setup, queue_depth);

  queue_setup->owner = AR_QUEUE_PRODUCER;
  ar_status_t status = ar_queue_create(queue_setup, &ar_queue_produce);
  EXPECT_TRUE(status == AR_OK);

  queue_setup->owner = AR_QUEUE_CONSUMER;
  ar_queue_create(queue_setup, &ar_queue_consume);
  EXPECT_TRUE(status == AR_OK);

  ar_queues_created = true;
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestRequiredSize, wearables_interconnect) {
  ar_queue_setup_t queue_setup;
  create_valid_setup(&queue_setup, /*queue_depth=*/c_queue_depth);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestQueueCreate, wearables_interconnect) {
  ar_queue_setup_t queue_setup;
  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestBasic, wearables_interconnect) {
  uint32_t produce_idx = 0;
  uint32_t consume_idx = 0;
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);

  for (uint32_t i = 0; i < c_queue_depth; i++) {
    packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(packet != nullptr);
    EXPECT_TRUE(produce_idx == i);

    packet->data[0] = c_data_prefix + i;

    ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);
  }
  packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(packet == nullptr);

  for (uint32_t i = 0; i < c_queue_depth; i++) {
    packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
    EXPECT_TRUE(packet != nullptr);
    EXPECT_TRUE(consume_idx == i);
    EXPECT_TRUE(packet->data[0] == c_data_prefix + i);

    ar_queue_mark_element_produce_ready(&ar_queue_consume, consume_idx);
  }
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestIsEmpty, wearables_interconnect) {
  uint32_t produce_idx = 0;
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;
  bool empty;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);
  empty = ar_queue_is_empty(&ar_queue_produce);
  EXPECT_TRUE(empty);

  packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(packet != nullptr);

  ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);
  empty = ar_queue_is_empty(&ar_queue_produce);
  EXPECT_FALSE(empty);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestGetProducerInfo, wearables_interconnect) {
  uint32_t produce_idx = 0;
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;
  ar_queue_producer_info_t info;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);
  info = ar_queue_get_producer_info(&ar_queue_produce);
  EXPECT_TRUE(info.index == 0);

  packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(packet != nullptr);

  ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);
  info = ar_queue_get_producer_info(&ar_queue_produce);
  EXPECT_TRUE(info.index == 1);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestGetConsumerInfo, wearables_interconnect) {
  uint32_t consume_idx = 0;
  uint32_t produce_idx = 0;
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;
  ar_queue_consumer_info_t info;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);
  info = ar_queue_get_consumer_info(&ar_queue_consume);
  EXPECT_TRUE(info.index == 0);

  packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(packet != nullptr);

  ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);

  packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
  EXPECT_TRUE(packet != nullptr);
  info = ar_queue_get_consumer_info(&ar_queue_consume);
  EXPECT_TRUE(info.index == 1);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestPacketAvailable, wearables_interconnect) {
  uint32_t produce_idx = 0;
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;
  bool available;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);
  available = ar_queue_packet_available(&ar_queue_consume);
  EXPECT_FALSE(available);

  packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(packet != nullptr);

  ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);

  available = ar_queue_packet_available(&ar_queue_consume);
  EXPECT_TRUE(available);
}

FRL_TEST_F_ONCALL(ArQueueTest, ArQueueTestDump, wearables_interconnect) {
  uint32_t produce_idx = 0;
  uint32_t consume_idx = 0;
  char dump[1000] = {};
  ar_packet_t* packet;
  ar_queue_setup_t queue_setup;
  int err;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);

  for (uint32_t i = 0; i < c_queue_depth / 2; i++) {
    packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(packet != nullptr);
    EXPECT_TRUE(produce_idx == i);

    packet->data[0] = c_data_prefix + i;

    ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);
  }
  err = ar_queue_dump(&ar_queue_produce, dump, 1000);
  EXPECT_TRUE(!err);

  for (uint32_t i = 0; i < c_queue_depth / 4; i++) {
    packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
    EXPECT_TRUE(packet != nullptr);
    EXPECT_TRUE(consume_idx == i);
    EXPECT_TRUE(packet->data[0] == c_data_prefix + i);

    ar_queue_mark_element_produce_ready(&ar_queue_consume, consume_idx);
  }
  err = ar_queue_dump(&ar_queue_consume, dump, 1000);
  EXPECT_TRUE(!err);
}

FRL_TEST_F_ONCALL(ArQueueTest, GenericWrapIterationAround, wearables_interconnect) {
  ar_queue_setup_t queue_setup;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);

  for (uint32_t i = 0; i < AR_QUEUE_ITERATION_COUNT * 2; i++) {
    // Produce the packet.
    ar_packet_t* produce_packet;
    uint32_t produce_idx = UINT32_MAX;
    produce_packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(produce_packet != NULL);

    produce_packet->data[0] = c_data_prefix + i;
    ar_queue_mark_element_consume_ready(&ar_queue_produce, produce_idx);

    // Consume packet.
    ar_packet_t* consume_packet;
    uint32_t consume_idx = UINT32_MAX;
    consume_packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
    EXPECT_TRUE(consume_packet != NULL);
    EXPECT_TRUE(consume_idx == produce_idx);
    EXPECT_TRUE(consume_packet == produce_packet);
    EXPECT_TRUE(consume_packet->data[0] = c_data_prefix + i);

    ar_queue_mark_element_produce_ready(&ar_queue_consume, consume_idx);
  }
}

FRL_TEST_F_ONCALL(ArQueueTest, ReserveAllUseFirstReserveAgain, wearables_interconnect) {
  ar_queue_setup_t queue_setup;
  ar_packet_t *produce_packet, *first_packet = nullptr;
  uint32_t produce_idx = UINT32_MAX;

  create_queues(&queue_setup, /*queue_depth=*/c_queue_depth);

  // Reserve all
  for (uint32_t i = 0; i < c_queue_depth; i++) {
    // Produce the packet.
    produce_packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(produce_packet != NULL);

    if (i == 0) {
      first_packet = produce_packet;
      first_packet->data[0] = c_data_prefix;
    }
  }

  // Consume the first packet
  ar_queue_mark_element_consume_ready(&ar_queue_produce, 0);

  // Produce the first packet.
  ar_packet_t* consume_packet;
  uint32_t consume_idx = UINT32_MAX;
  consume_packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
  EXPECT_TRUE(consume_packet != NULL);
  EXPECT_TRUE(consume_idx == 0);
  EXPECT_TRUE(consume_packet->data[0] == c_data_prefix);

  // Mark it consumed
  ar_queue_mark_element_produce_ready(&ar_queue_consume, consume_idx);

  // Verify we can reserve the consumed packet
  produce_packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
  EXPECT_TRUE(produce_packet != NULL);
  EXPECT_TRUE(produce_idx == 0);
}

FRL_TEST_F_ONCALL(ArQueueTest, WrapUpIterationCounter, wearables_interconnect) {
  ar_queue_setup_t queue_setup;
  ar_packet_t *produce_packet, *first_packet = nullptr;
  uint32_t produce_idx = UINT32_MAX;

  create_queues(&queue_setup, c_queue_depth);

  // Reserve all
  for (uint32_t i = 0; i < c_queue_depth; i++) {
    // Produce the packet.
    produce_packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(produce_packet != NULL);

    first_packet = produce_packet;
    // first_packet->data[i] = c_data_prefix + i;
  }

  for (uint32_t i = 0; i < (c_queue_depth * UINT16_MAX + 1); i++) {
    // Consume the packet
    ar_queue_mark_element_consume_ready(&ar_queue_produce, i % c_queue_depth);

    // Produce the  packet.
    ar_packet_t* consume_packet;
    uint32_t consume_idx = UINT32_MAX;
    consume_packet = (ar_packet_t*)ar_queue_get_next_consume(&ar_queue_consume, &consume_idx);
    EXPECT_TRUE(consume_packet != NULL);

    // Mark it consumed
    ar_queue_mark_element_produce_ready(&ar_queue_consume, consume_idx);

    // Verify we can reserve the consumed packet
    produce_packet = ar_queue_get_next_produce(&ar_queue_produce, &produce_idx);
    EXPECT_TRUE(produce_packet != NULL);
  }
}
