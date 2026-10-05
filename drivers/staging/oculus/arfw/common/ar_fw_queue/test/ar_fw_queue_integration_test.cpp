#include <FrlGtestWrapper.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>

#include "ar_fw_queue.h"
#include "ar_queue.h"
#include "ar_queue_consumer.h"
#include "ar_utils.h"

static constexpr uint32_t default_depth = 32;
static constexpr uint32_t default_element_size = 32;

static constexpr uint32_t default_dcache_line_size = 16;
static constexpr uint32_t default_page_size = 4096;
static constexpr std::size_t loop_iterations = 32;

using namespace ::testing;

class ArFwQueueIntegrationTest : public ::testing::Test {
 public:
  ArFwQueueIntegrationTest() = default;
  ~ArFwQueueIntegrationTest() = default;

  static bool test_queue_cb(
      void* callback_context,
      ar_fw_io_request_t* request,
      void* request_context,
      bool* mark_consumed) {
    auto thisp = reinterpret_cast<ArFwQueueIntegrationTest*>(callback_context);

    if (thisp && thisp->request_validator_) {
      thisp->request_validator_(request, request_context);
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

  /**
   * Setup the default queue pair either in the SEND or RECEIVE direction.
   */
  void SetupQueuePair(
      queue_direction_t queue_direction,
      ar_fw_queue_t& client_queue,
      ar_fw_request_callback_t client_cb,
      ar_fw_queue_t& driver_queue,
      ar_fw_request_callback_t driver_cb) {
    int status;

    // setup queue meta object
    ar_fw_queue_meta_t meta;
    status = ar_fw_queue_meta_init(
        default_depth,
        default_element_size,
        queue_direction,
        default_dcache_line_size,
        default_page_size,
        &meta);
    ASSERT_EQ(status, 0);

    ASSERT_EQ(meta.queue_direction, queue_direction);
    ASSERT_EQ(meta.element_size, default_element_size);
    ASSERT_EQ(meta.depth, default_depth);

    // allocate buf memory
    std::size_t meta_size, data_size;
    uintptr_t meta_mem_base, data_mem_base;
    meta_size = meta.produce_meta_size + meta.consume_meta_size + meta.page_size;
    meta_mem_.resize(meta_size + default_page_size);
    meta_mem_base = AR_ROUNDUP((uintptr_t)meta_mem_.data(), default_page_size);

    data_size = meta.data_size;

    data_mem_.resize(data_size + default_page_size);
    data_mem_base = AR_ROUNDUP((uintptr_t)data_mem_.data(), default_page_size);

    // create client queue
    ar_fw_queue_creator_data_t client_params;
    ar_fw_queue_creator_data_client_init(&client_params, client_cb, this);

    status = ar_fw_queue_create(
        &meta, meta_mem_base, meta_size, data_mem_base, data_size, &client_params, &client_queue);
    ASSERT_EQ(status, 0);

    // create driver queue
    ar_fw_queue_creator_data_t driver_params;
    ar_fw_queue_creator_data_driver_init(&driver_params, driver_cb, this);

    status = ar_fw_queue_create(
        &meta,
        meta_mem_base,
        meta_size,
        data_mem_base,
        meta.total_mem_size,
        &driver_params,
        &driver_queue);

    ASSERT_EQ(status, 0);
  }

  std::vector<char> meta_mem_;
  std::vector<char> data_mem_;
  std::function<void(ar_fw_io_request_t*, void*)> request_validator_;
};

FRL_TEST_F_ONCALL(ArFwQueueIntegrationTest, TestRecvQueuePairConstruction, wearables_interconnect) {
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  SetupQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, nullptr);

  basic_queue_pair_validation(client_queue, driver_queue);

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 0u);
  EXPECT_EQ(ar_fw_queue_get_consumable_count(&driver_queue), 0u);

  ar_fw_queue_destroy(&client_queue);
  ar_fw_queue_destroy(&driver_queue);
}

FRL_TEST_F_ONCALL(ArFwQueueIntegrationTest, TestSendQueuePairConstruction, wearables_interconnect) {
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  // create queue pair
  SetupQueuePair(SEND, client_queue, nullptr, driver_queue, test_queue_cb);

  // validate queue pair
  basic_queue_pair_validation(client_queue, driver_queue);

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 0u);
  EXPECT_EQ(ar_fw_queue_get_consumable_count(&driver_queue), 0u);

  ar_fw_queue_destroy(&client_queue);
  ar_fw_queue_destroy(&driver_queue);
}

FRL_TEST_F_ONCALL(ArFwQueueIntegrationTest, TestRecvQueueData, wearables_interconnect) {
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  SetupQueuePair(RECEIVE, client_queue, test_queue_cb, driver_queue, nullptr);

  basic_queue_pair_validation(client_queue, driver_queue);

  EXPECT_EQ(ar_fw_queue_get_consumable_count(&client_queue), 0u);
  EXPECT_EQ(ar_fw_queue_get_consumable_count(&driver_queue), 0u);

  for (std::size_t loop_count = 0; loop_count < loop_iterations; loop_count++) {
    // fill the queue with patterns
    for (unsigned temp = 0; temp < default_depth; temp++) {
      ar_fw_io_request_t req;
      ASSERT_EQ(ar_fw_queue_get_consumable_count(&client_queue), temp);
      ASSERT_EQ(ar_fw_queue_slot_reserve(&driver_queue, &req), 0);
      ASSERT_EQ(ar_fw_queue_get_consumable_count(&client_queue), temp);

      ASSERT_EQ(req.index, temp);
      ASSERT_EQ(req.data_size, default_element_size);
      memset(req.data, (uint8_t)temp, req.data_size);

      ar_fw_queue_slot_produce(&driver_queue, &req);

      ASSERT_EQ(ar_fw_queue_get_consumable_count(&client_queue), temp + 1);
    }

    // check the queue
    unsigned counter = 0;
    request_validator_ = [&](ar_fw_io_request_t* req, void*) {
      ASSERT_EQ(counter, req->index);
      ASSERT_EQ(req->data_size, default_element_size);

      for (unsigned temp = 0; temp < req->data_size; temp++) {
        ASSERT_EQ(((uint8_t*)req->data)[temp], (uint8_t)counter);
      }

      counter++;
      ASSERT_EQ(ar_fw_queue_get_consumable_count(&client_queue), default_depth - counter);
    };

    ASSERT_EQ(ar_fw_queue_poll(&client_queue, default_depth, nullptr), default_depth);
  }

  ar_fw_queue_destroy(&client_queue);
  ar_fw_queue_destroy(&driver_queue);
}

FRL_TEST_F_ONCALL(ArFwQueueIntegrationTest, TestSendQueueData, wearables_interconnect) {
  ar_fw_queue_t client_queue;
  ar_fw_queue_t driver_queue;

  SetupQueuePair(SEND, client_queue, nullptr, driver_queue, test_queue_cb);

  basic_queue_pair_validation(client_queue, driver_queue);

  for (std::size_t loop_count = 0; loop_count < loop_iterations; loop_count++) {
    // fill the queue with patterns
    for (unsigned temp = 0; temp < default_depth; temp++) {
      ar_fw_io_request_t req;
      ASSERT_EQ(ar_fw_queue_slot_reserve(&client_queue, &req), 0);

      ASSERT_EQ(req.index, temp);
      ASSERT_EQ(req.data_size, default_element_size);
      memset(req.data, (uint8_t)temp, req.data_size);

      ar_fw_queue_slot_produce(&client_queue, &req);
    }

    // check the queue
    unsigned counter = 0;
    request_validator_ = [&](ar_fw_io_request_t* req, void*) {
      ASSERT_EQ(counter, req->index);
      ASSERT_EQ(req->data_size, default_element_size);

      for (unsigned temp = 0; temp < req->data_size; temp++) {
        ASSERT_EQ(((uint8_t*)req->data)[temp], (uint8_t)counter);
      }

      counter++;
    };

    ASSERT_EQ(ar_fw_queue_poll(&driver_queue, default_depth, nullptr), default_depth);
  }

  ar_fw_queue_destroy(&client_queue);
  ar_fw_queue_destroy(&driver_queue);
}
