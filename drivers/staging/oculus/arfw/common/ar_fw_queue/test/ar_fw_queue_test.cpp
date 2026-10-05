// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <cstdlib>
#include <memory>
#include <utility>

#include <FrlGtestWrapper.h>
#include <gmock/gmock-actions.h>
#include <gmock/gmock-spec-builders.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ar_fw_queue.h"
#include "mock_circular_queue.h"

static const uint32_t c_ar_fw_test_element_size = 32;

// arbitrary probably big enough size for tests
static const size_t default_cq_req_size = 16 * 1024;

static const uint32_t default_dcache_line_size = 16;
static const uint32_t default_page_size = 4096;
static const uint32_t default_align_size = default_page_size;

using namespace ::testing;

static void validate_arfw_queue(const ar_fw_queue_meta_t& meta, ar_fw_queue_t& queue) {
  EXPECT_EQ(meta.depth, queue.size);
  EXPECT_EQ(meta.element_size, queue.submit_element_size);
}

class ArFirmwareQueueTest : public ::testing::Test {
 public:
  CircularQueueMock cq_mock;
  ArFirmwareQueueTest() = default;
  ~ArFirmwareQueueTest() = default;

  void SetUp() override {
    g_cq_mock = &cq_mock;
  }

  void TearDown() override {
    g_cq_mock = nullptr;
  }

  void ExpectArQueueRequiredSize(
      std::size_t element_size,
      std::uint16_t depth,
      std::size_t align,
      std::size_t count) {
    EXPECT_CALL(cq_mock, CircularQueueRequiredSize(element_size, depth, align, _, _, _))
        .Times(count)
        .WillRepeatedly(WithArgs<3, 4, 5>(
            Invoke([](std::size_t* p, std::size_t* c, std::size_t* d) -> ar_status_t {
              *p = default_cq_req_size;
              *c = default_cq_req_size;
              *d = default_cq_req_size;

              return AR_OK;
            })));
  }

  void ExpectArQueueRequiredSize(std::size_t count) {
    ExpectArQueueRequiredSize(
        c_ar_fw_test_element_size, c_ar_fw_default_queue_depth, default_align_size, count);
  }

  void ExpectArQueuePairCreate(
      std::size_t element_size,
      std::size_t depth,
      queue_direction_t direction,
      ar_fw_queue_t& client_queue,
      ar_fw_queue_t& driver_queue) {
    EXPECT_CALL(cq_mock, CircularQueueCreate(_, &client_queue.submit_queue))
        .WillOnce(Invoke([element_size, depth, direction](ar_queue_setup_t* setup, Unused) {
          EXPECT_EQ(setup->owner, direction == SEND ? AR_QUEUE_PRODUCER : AR_QUEUE_CONSUMER);
          EXPECT_EQ(setup->element_size, element_size);
          EXPECT_EQ(setup->element_count, depth);
          return AR_OK;
        }));
    EXPECT_CALL(cq_mock, CircularQueueCreate(_, &driver_queue.submit_queue))
        .WillOnce(Invoke([element_size, depth, direction](ar_queue_setup_t* setup, Unused) {
          EXPECT_EQ(setup->owner, direction == SEND ? AR_QUEUE_CONSUMER : AR_QUEUE_PRODUCER);
          EXPECT_EQ(setup->element_size, element_size);
          EXPECT_EQ(setup->element_count, depth);
          return AR_OK;
        }));
  }

  void ExpectArQueuePairCreate(
      queue_direction_t direction,
      ar_fw_queue_t& client_queue,
      ar_fw_queue_t& driver_queue) {
    ExpectArQueuePairCreate(
        c_ar_fw_test_element_size,
        c_ar_fw_default_queue_depth,
        direction,
        client_queue,
        driver_queue);
  }

  void ExpectQueuePairCreate(
      queue_direction_t direction,
      ar_fw_queue_t& client_queue,
      ar_fw_queue_t& driver_queue) {
    /**
     * Expect 1 calls per queue, plus 1 for meta creation. (total 3)
     */
    ExpectArQueueRequiredSize(3);

    /**
     * Should be creating a producer consumer pair always.
     */
    ExpectArQueuePairCreate(direction, client_queue, driver_queue);
  }

  /**
   * Setup the default queue pair either in the SEND or RECEIVE direction.
   */
  void SetupDefaultQueuePair(
      queue_direction_t queue_direction,
      ar_fw_queue_t& client_queue,
      ar_fw_request_callback_t client_cb,
      ar_fw_queue_t& driver_queue,
      ar_fw_request_callback_t driver_cb,
      std::vector<char>& shared_mem) {
    int status;

    // setup queue meta object
    ar_fw_queue_meta_t meta;
    status = ar_fw_queue_meta_init(
        c_ar_fw_default_queue_depth,
        c_ar_fw_test_element_size,
        queue_direction,
        default_dcache_line_size,
        default_page_size,
        &meta);
    ASSERT_EQ(status, 0);

    ASSERT_EQ(meta.queue_direction, queue_direction);
    ASSERT_EQ(meta.element_size, c_ar_fw_test_element_size);
    ASSERT_EQ(meta.depth, c_ar_fw_default_queue_depth);

    // allocate buf memory
    shared_mem.resize(meta.total_mem_size);

    // create client queue
    ar_fw_queue_creator_data_t client_params;
    ar_fw_queue_creator_data_client_init(&client_params, client_cb, this);

    std::size_t meta_size;
    meta_size = meta.produce_meta_size + meta.consume_meta_size;
    status = ar_fw_queue_create(
        &meta,
        (uintptr_t)shared_mem.data(),
        meta_size,
        (uintptr_t)shared_mem.data() + meta_size + meta.page_size,
        meta.data_size,
        &client_params,
        &client_queue);
    ASSERT_EQ(status, 0);
    validate_arfw_queue(meta, client_queue);

    // create driver queue
    ar_fw_queue_creator_data_t driver_params;
    ar_fw_queue_creator_data_driver_init(&driver_params, driver_cb, this);

    status = ar_fw_queue_create(
        &meta,
        (std::uintptr_t)shared_mem.data(),
        meta_size,
        (std::uintptr_t)shared_mem.data() + meta_size + meta.page_size,
        meta.data_size,
        &driver_params,
        &driver_queue);

    ASSERT_EQ(status, 0);
    validate_arfw_queue(meta, driver_queue);
  }
};

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestClientReceiveWithoutCallback, wearables_interconnect) {
  int err;
  char fake_buf[8];

  /**
   * Expect required size twice.
   */
  ExpectArQueueRequiredSize(2);

  // client recv without callback should fail
  ar_fw_queue_meta_t meta;
  err = ar_fw_queue_meta_init(
      c_ar_fw_default_queue_depth,
      c_ar_fw_test_element_size,
      RECEIVE,
      default_dcache_line_size,
      default_page_size,
      &meta);
  ASSERT_EQ(err, 0);

  ar_fw_queue_t queue;
  ar_fw_queue_creator_data_t client_params;
  ar_fw_queue_creator_data_client_init(&client_params, nullptr, nullptr);

  std::size_t meta_size;
  meta_size = meta.produce_meta_size + meta.consume_meta_size;
  err = ar_fw_queue_create(
      &meta,
      (uintptr_t)fake_buf,
      meta_size,
      (uintptr_t)fake_buf + meta_size + meta.page_size,
      meta.data_size,
      &client_params,
      &queue);

  ASSERT_NE(err, 0);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestDriverReceiveWithoutCallback, wearables_interconnect) {
  int err;
  char fake_buf[8];

  /**
   * Expect required size twice.
   */
  ExpectArQueueRequiredSize(2);

  // driver send queue without callback should fail.
  ar_fw_queue_meta_t meta;
  err = ar_fw_queue_meta_init(
      c_ar_fw_default_queue_depth,
      c_ar_fw_test_element_size,
      SEND,
      default_dcache_line_size,
      default_page_size,
      &meta);
  ASSERT_EQ(err, 0);

  ar_fw_queue_t queue;
  ar_fw_queue_creator_data_t driver_params;
  ar_fw_queue_creator_data_driver_init(&driver_params, nullptr, nullptr);

  std::size_t meta_size;
  meta_size = meta.produce_meta_size + meta.consume_meta_size;
  err = ar_fw_queue_create(
      &meta,
      (uintptr_t)fake_buf,
      meta_size,
      (uintptr_t)fake_buf + meta_size + meta.page_size,
      meta.data_size,
      &driver_params,
      &queue);

  ASSERT_NE(err, 0);
}

static bool test_queue_cb(
    void* callback_context,
    ar_fw_io_request_t* request,
    void* request_context,
    bool* mark_consumed) {
  std::ignore = callback_context;

  if (request->data != request_context) {
    EXPECT_EQ(memcmp(request->data, request_context, request->data_size), 0);
  }

  *mark_consumed = true;
  return true;
}

static void basic_queue_pair_validation(
    const ar_fw_queue_t& client_queue,
    const ar_fw_queue_t& driver_queue) {
  // driver queue should never equal client queue
  ASSERT_NE(&client_queue, &driver_queue);

  // ar_fw_queue reported sizes should match
  EXPECT_EQ(client_queue.size, driver_queue.size);
  EXPECT_EQ(client_queue.submit_element_size, driver_queue.submit_element_size);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestRecvQueuePairConstruction, wearables_interconnect) {
  std::vector<char> shared_mem;
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // setup queue pair creation expectations
  ExpectQueuePairCreate(RECEIVE, client_queue, driver_queue);

  // create queue pair
  SetupDefaultQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, NULL, shared_mem);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestSendQueuePairConstruction, wearables_interconnect) {
  std::vector<char> shared_mem;
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // setup queue pair creation expectations
  ExpectQueuePairCreate(SEND, client_queue, driver_queue);

  // create queue pair
  SetupDefaultQueuePair(SEND, client_queue, NULL, driver_queue, test_queue_cb, shared_mem);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestRecvQueuePairPoll, wearables_interconnect) {
  std::vector<char> shared_mem;
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // setup queue pair creation expectations
  ExpectQueuePairCreate(RECEIVE, client_queue, driver_queue);

  // setup polling, poll once then have no more data
  {
    InSequence s;
    EXPECT_CALL(cq_mock, CircularQueueGetNextConsume(&client_queue.submit_queue, _))
        .WillOnce(Invoke([&shared_mem](Unused, uint32_t* index) {
          *index = 0;
          return shared_mem.data();
        }));
  }

  // we will have mark_consumed = true, so expect the produce ready call
  EXPECT_CALL(cq_mock, CircularQueueMarkElementProduceReady(&client_queue.submit_queue, _))
      .WillOnce(Invoke([](Unused, uint32_t index) {
        EXPECT_EQ(index, 0u);
        return AR_OK;
      }));

  // create queue pair
  SetupDefaultQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, NULL, shared_mem);

  auto test_consumer_info = std::make_unique<ar_queue_consumer_info_t>();
  auto test_producer_info = std::make_unique<ar_queue_producer_info_t>();
  EXPECT_CALL(cq_mock, CircularQueueGetConsumerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_consumer_info.get()](Unused) { return *info; }));
  EXPECT_CALL(cq_mock, CircularQueueGetProducerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_producer_info.get()](Unused) { return *info; }));

  // bump up producer index, adding one element to the queue
  test_producer_info->index = 1;

  // poll on the queue
  constexpr size_t poll_count = 2;
  constexpr size_t expected_poll_total = 1;
  EXPECT_EQ(ar_fw_queue_poll(&client_queue, poll_count, shared_mem.data()), expected_poll_total);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestRecvQueuePairNoConsume, wearables_interconnect) {
  std::vector<char> shared_mem;
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // setup queue pair creation expectations
  ExpectQueuePairCreate(RECEIVE, client_queue, driver_queue);

  // setup polling, poll once then have no more data
  {
    InSequence s;
    EXPECT_CALL(cq_mock, CircularQueueGetNextConsume(&client_queue.submit_queue, _))
        .WillOnce(Invoke([](Unused, Unused) { return nullptr; }));
  }

  // create queue pair
  SetupDefaultQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, NULL, shared_mem);

  auto test_consumer_info = std::make_unique<ar_queue_consumer_info_t>();
  auto test_producer_info = std::make_unique<ar_queue_producer_info_t>();
  EXPECT_CALL(cq_mock, CircularQueueGetConsumerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_consumer_info.get()](Unused) { return *info; }));
  EXPECT_CALL(cq_mock, CircularQueueGetProducerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_producer_info.get()](Unused) { return *info; }));

  // bump up producer index, adding one element to the queue
  test_producer_info->index = 1;

  // poll on the queue
  constexpr size_t poll_count = 2;
  constexpr size_t expected_poll_total = 0;
  EXPECT_EQ(ar_fw_queue_poll(&client_queue, poll_count, shared_mem.data()), expected_poll_total);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);
}

FRL_TEST_F_ONCALL(ArFirmwareQueueTest, TestGetConsumeableAmount, wearables_interconnect) {
  std::vector<char> shared_mem;
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // setup queue pair creation expectations
  ExpectQueuePairCreate(RECEIVE, client_queue, driver_queue);

  // create queue pair
  SetupDefaultQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, NULL, shared_mem);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);

  auto test_consumer_info = std::make_unique<ar_queue_consumer_info_t>();
  auto test_producer_info = std::make_unique<ar_queue_producer_info_t>();
  EXPECT_CALL(cq_mock, CircularQueueGetConsumerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_consumer_info.get()](Unused) { return *info; }));
  EXPECT_CALL(cq_mock, CircularQueueGetProducerInfo(&client_queue.submit_queue))
      .WillRepeatedly(Invoke([info = test_producer_info.get()](Unused) { return *info; }));

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 0);

  // bump up producer index, adding one element to the queue
  test_producer_info->index = 1;

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 1);

  // bump up consumer index, draining the queue
  test_consumer_info->index = 1;

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 0);

  // bump up the producer iteration, filling the entire queue
  test_producer_info->iteration++;

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), c_ar_fw_default_queue_depth);
}
