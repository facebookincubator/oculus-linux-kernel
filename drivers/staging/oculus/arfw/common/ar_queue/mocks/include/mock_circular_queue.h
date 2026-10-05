/*
 * This software contains information and intellectual property
 * that is confidential and proprietary to Facebook, Inc. and its affiliates.
 */

/** @file */

#pragma once

#include <chrono>
#include <string>
#include <thread>

#include <gmock/gmock.h>
#include "ar_queue_consumer.h"

class CircularQueueMock {
 public:
  MOCK_METHOD(ar_status_t, CircularQueueCreate, (ar_queue_setup_t*, ar_queue_t*));
  MOCK_METHOD(bool, CircularQueueIsEmpty, (ar_queue_t*));
  MOCK_METHOD(void, CircularQueueDestroy, (ar_queue_t*));
  MOCK_METHOD(
      ar_status_t,
      CircularQueueRequiredSize,
      (size_t, uint16_t, size_t, size_t*, size_t*, size_t*));
  MOCK_METHOD(ar_packet_t*, CircularQueueGetNextProduce, (ar_queue_t*, uint32_t*));
  MOCK_METHOD(void*, CircularQueueGetNextConsume, (ar_queue_t*, uint32_t*));
  MOCK_METHOD(ar_status_t, CircularQueueMarkElementConsumeReady, (ar_queue_t*, uint32_t));
  MOCK_METHOD(ar_status_t, CircularQueueMarkElementProduceReady, (ar_queue_t*, uint32_t));
  MOCK_METHOD(ar_queue_consumer_info_t, CircularQueueGetConsumerInfo, (const ar_queue_t*));
  MOCK_METHOD(ar_queue_producer_info_t, CircularQueueGetProducerInfo, (const ar_queue_t*));
};

extern CircularQueueMock* g_cq_mock;
