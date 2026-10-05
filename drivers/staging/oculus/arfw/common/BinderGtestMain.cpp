// Copyright (c) Meta Technologies, LLC and its affiliates. All Rights reserved.

#include <android/binder_process.h>
#include <gtest/gtest.h>

int main(int argc, char** argv) {
  ABinderProcess_setThreadPoolMaxThreadCount(1);
  ABinderProcess_startThreadPool();

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
