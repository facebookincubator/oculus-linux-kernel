// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <chrono>
#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include <FrlGtestWrapper.h>

#include "ar_binned_sampler.h"

class ArBinnedSamplerTests : public ::testing::Test {
 protected:
  void SetupBinnedSampler(const std::vector<int>& thresholds) {
    const auto required_size = AR_BINNED_SAMPLER_SIZE(thresholds.size());
    bs_mem_.resize(required_size);
    bs_ = reinterpret_cast<ar_binned_sampler_t*>(bs_mem_.data());

    ar_binned_sampler_init(bs_, thresholds.data(), thresholds.size());
  }

  std::vector<char> bs_mem_;
  ar_binned_sampler_t* bs_;
};

FRL_TEST_F_ONCALL(ArBinnedSamplerTests, ArBinnedSamplerConstructionTest, wearables_interconnect) {
  std::vector<int> thresholds(1, 1);
  SetupBinnedSampler(thresholds);

  EXPECT_EQ(ar_binned_sampler_get_min(bs_), -1);
  EXPECT_EQ(ar_binned_sampler_get_max(bs_), -1);
  EXPECT_EQ(ar_binned_sampler_get_num_samples(bs_), 0u);
  EXPECT_EQ(ar_binned_sampler_get_threshold(bs_, 1), -1);
}

FRL_TEST_F_ONCALL(ArBinnedSamplerTests, BinnedSampleSingleThresholdTests, wearables_interconnect) {
  constexpr int kSingleThreshold = 10;
  std::vector<int> thresholds;
  thresholds.push_back(kSingleThreshold);

  // in this case, all of the data is within the first threshold
  {
    constexpr int minSample = 0;
    constexpr int maxSample = kSingleThreshold - 1;

    SetupBinnedSampler(thresholds);
    for (int temp = minSample; temp <= maxSample; temp++) {
      ar_binned_sampler_add_sample(bs_, temp);
    }

    for (int temp = 1; temp <= 100; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kSingleThreshold);
    }

    EXPECT_EQ(ar_binned_sampler_get_min(bs_), minSample);
    EXPECT_EQ(ar_binned_sampler_get_max(bs_), maxSample);
    EXPECT_EQ(
        ar_binned_sampler_get_num_samples(bs_), static_cast<uint32_t>(maxSample - minSample + 1));
  }

  // test samples beyound the maximum threshold
  {
    constexpr int largeSample = kSingleThreshold + 1;
    SetupBinnedSampler(thresholds);

    ar_binned_sampler_add_sample(bs_, largeSample);

    // check percentiles
    for (int temp = 1; temp < 100; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), largeSample);
    }

    EXPECT_EQ(ar_binned_sampler_get_min(bs_), largeSample);
    EXPECT_EQ(ar_binned_sampler_get_max(bs_), largeSample);
    EXPECT_EQ(ar_binned_sampler_get_num_samples(bs_), 1u);
  }
}

FRL_TEST_F_ONCALL(ArBinnedSamplerTests, BinnedSampleDoubleThresholdTests, wearables_interconnect) {
  constexpr int kFirstThreshold = 10;
  constexpr int kSecondThreshold = 20;
  std::vector<int> thresholds;
  thresholds.push_back(kFirstThreshold);
  thresholds.push_back(kSecondThreshold);

  // in this case, all of the data is within the first threshold
  {
    constexpr int minSample = 0;
    constexpr int maxSample = kFirstThreshold - 1;

    SetupBinnedSampler(thresholds);
    for (int temp = minSample; temp <= maxSample; temp++) {
      ar_binned_sampler_add_sample(bs_, temp);
    }

    for (int temp = 1; temp <= 100; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kFirstThreshold);
    }

    EXPECT_EQ(ar_binned_sampler_get_min(bs_), minSample);
    EXPECT_EQ(ar_binned_sampler_get_max(bs_), maxSample);
    EXPECT_EQ(
        ar_binned_sampler_get_num_samples(bs_), static_cast<uint32_t>(maxSample - minSample + 1));
  }

  // all values in the second threshold
  {
    constexpr int minSample = kFirstThreshold;
    constexpr int maxSample = kSecondThreshold - 1;

    SetupBinnedSampler(thresholds);
    for (int temp = minSample; temp <= maxSample; temp++) {
      ar_binned_sampler_add_sample(bs_, temp);
    }

    for (int temp = 1; temp <= 100; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kSecondThreshold);
    }

    EXPECT_EQ(ar_binned_sampler_get_min(bs_), minSample);
    EXPECT_EQ(ar_binned_sampler_get_max(bs_), maxSample);
    EXPECT_EQ(
        ar_binned_sampler_get_num_samples(bs_), static_cast<uint32_t>(maxSample - minSample + 1));
  }

  // one value in each threshold
  {
    SetupBinnedSampler(thresholds);

    ar_binned_sampler_add_sample(bs_, kFirstThreshold - 1);
    ar_binned_sampler_add_sample(bs_, kFirstThreshold + 1);

    for (int temp = 1; temp <= 50; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kFirstThreshold);
    }

    for (int temp = 51; temp <= 100; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kSecondThreshold);
    }
  }

  // one value in each threshold and one over the second threshold
  {
    SetupBinnedSampler(thresholds);

    ar_binned_sampler_add_sample(bs_, kFirstThreshold - 1);
    ar_binned_sampler_add_sample(bs_, kFirstThreshold + 1);
    ar_binned_sampler_add_sample(bs_, kSecondThreshold + 1);

    for (int temp = 1; temp <= 33; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kFirstThreshold);
    }

    for (int temp = 34; temp <= 66; temp++) {
      ASSERT_LE(ar_binned_sampler_get_threshold(bs_, temp), kSecondThreshold);
    }

    for (int temp = 67; temp <= 100; temp++) {
      ASSERT_GT(ar_binned_sampler_get_threshold(bs_, temp), kSecondThreshold);
    }
  }
}

FRL_TEST_F_ONCALL(ArBinnedSamplerTests, BinnedSampleSimpleThresholdTests, wearables_interconnect) {
  std::vector<int> thresholds;

  // how many elements we plan on putting into each threshold bin
  // also how many unique elements get mapped to a threshold bin
  constexpr int kBinSize = 10;

  // how many bins we are going to make.
  constexpr int kBinCount = 10;

  // create thresholds [10, 20, ..., 100]
  for (auto temp = 1; temp <= kBinCount; temp++) {
    thresholds.push_back(temp * kBinSize);
  }

  SetupBinnedSampler(thresholds);

  // add 0 - 99 as samples
  for (int temp = 0; temp < kBinSize * kBinCount; temp++) {
    ar_binned_sampler_add_sample(bs_, temp);
  }

  const uint64_t* counts = ar_binned_get_counts(bs_);
  // at this time, bin distribution is 10 10s, and one 0.
  for (auto temp = 0; temp < kBinCount; temp++) {
    EXPECT_EQ(counts[temp], static_cast<uint64_t>(kBinSize)) << "bin index: " << temp;
  }
  EXPECT_EQ(counts[kBinCount], 0u);

  // check min and max
  EXPECT_EQ(ar_binned_sampler_get_min(bs_), 0);
  EXPECT_EQ(ar_binned_sampler_get_max(bs_), kBinSize * kBinCount - 1);

  // we expect percentiles be between adjacent bins in [10, 20, 30, ... 100]
  int prev_threshold_value = 0;
  for (int temp = 1; temp < 100; temp++) {
    // low bins map [1, 10] -> 0, [11, 20] -> 10, ... [91 - 100] -> 90
    auto low_bin = ((temp - 1) / 10) * 10;
    // high bins map [1, 10] -> 10, [11, 20] -> 20, ... [91 - 100] -> 100
    auto high_bin = low_bin + 10;
    ASSERT_GE(ar_binned_sampler_get_threshold(bs_, temp), low_bin) << "percentile value: " << temp;
    EXPECT_LE(ar_binned_sampler_get_threshold(bs_, temp), high_bin) << "percentile value: " << temp;

    // we also expect threshold values to be non-decreasing
    EXPECT_GE(ar_binned_sampler_get_threshold(bs_, temp), prev_threshold_value)
        << "percentile value: " << temp;
    prev_threshold_value = ar_binned_sampler_get_threshold(bs_, temp);
  }

  // percentile 100 will be max value
  EXPECT_EQ(ar_binned_sampler_get_threshold(bs_, 100), ar_binned_sampler_get_max(bs_));

  /**
   * Part 2: go past the maximum threshold, and validate percentiles still look
   * correct. (also min and max)
   */
  // add kBinSize * kBinCount (100) samples of a value larger than maximum threshold
  for (int temp = 0; temp < kBinSize * kBinCount; temp++) {
    ar_binned_sampler_add_sample(bs_, kBinSize * kBinCount + 1);
  }

  // now we should have 100 (kBinSize * kBinCount) samples in the last bucket
  EXPECT_EQ(counts[kBinCount], static_cast<uint64_t>(kBinSize * kBinCount));

  // check min and max
  EXPECT_EQ(ar_binned_sampler_get_min(bs_), 0);
  EXPECT_EQ(ar_binned_sampler_get_max(bs_), kBinSize * kBinCount + 1);

  // percentiles up to 50 should fall between adjacent bins
  for (int temp = 1; temp <= 50; temp++) {
    // low bins map [1, 5] -> 0, [6, 10] -> 10, ... [46 - 50] -> 90
    auto low_bin = ((temp - 1) / 5) * 10;
    // high bins map [1, 5] -> 10, [6, 10] -> 20, ... [46 - 50] -> 100
    auto high_bin = low_bin + 10;
    EXPECT_GE(ar_binned_sampler_get_threshold(bs_, temp), low_bin) << "percentile value: " << temp;
    EXPECT_LE(ar_binned_sampler_get_threshold(bs_, temp), high_bin) << "percentile value: " << temp;
  }

  // percentiles from 51 to 99 be between max threshold and max value
  for (int temp = 51; temp < 100; temp++) {
    EXPECT_GE(ar_binned_sampler_get_threshold(bs_, temp), thresholds.back())
        << "percentile value: " << temp;
    EXPECT_LE(ar_binned_sampler_get_threshold(bs_, temp), kBinSize * kBinCount + 1)
        << "percentile value: " << temp;
  }

  // percentile 100 will be max value
  EXPECT_EQ(ar_binned_sampler_get_threshold(bs_, 100), ar_binned_sampler_get_max(bs_));
}

FRL_TEST_F_ONCALL(
    ArBinnedSamplerTests,
    BinnedSampleBinomialMt19937ThresholdTests,
    wearables_interconnect) {
  std::vector<int> thresholds;

  // how many elements we plan on putting into each threshold bin
  // also how many unique elements get mapped to a threshold bin
  constexpr int kBinSize = 10;

  // how many bins we are going to make.
  constexpr int kBinCount = 10;

  // we want many samples that mostly fit within thresholds, but with some outliers
  constexpr int kNumSamples = 10000;
  constexpr auto kBinomialCoefficient = 0.5;
  constexpr int kMaxSampleValue = kBinSize * kBinCount * (1 / kBinomialCoefficient) - kBinSize;

  // create thresholds [10, 20, ..., 100]
  for (auto temp = 1; temp <= kBinCount; temp++) {
    thresholds.push_back(temp * kBinSize);
  }

  // generate "random" numbers between 1 and 1000
  std::mt19937 generator(0);
  std::binomial_distribution<int> distribution(kMaxSampleValue, kBinomialCoefficient);

  SetupBinnedSampler(thresholds);

  std::vector<int> real_samples;
  for (int temp = 0; temp < kNumSamples; temp++) {
    real_samples.push_back(distribution(generator));
  }

  auto ingest_start = std::chrono::steady_clock::now();
  for (auto sample : real_samples) {
    ar_binned_sampler_add_sample(bs_, sample);
  }
  auto ingest_duration_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                std::chrono::steady_clock::now() - ingest_start)
                                .count();
  std::cout << "Ingesting " << kNumSamples << " samples took " << ingest_duration_us << " us\n";

  std::sort(real_samples.begin(), real_samples.end());

  // check that our binned sampler data values match
  EXPECT_EQ(ar_binned_sampler_get_min(bs_), real_samples.front());
  EXPECT_EQ(ar_binned_sampler_get_max(bs_), real_samples.back());
  EXPECT_EQ(ar_binned_sampler_get_num_samples(bs_), static_cast<uint32_t>(real_samples.size()));

  // check that our percentile values approximately match (rounded up to threshold, or max)
  int prev_threshold_value = 0;
  for (int temp = 1; temp <= 100; temp++) {
    auto real_sample_index = (temp * real_samples.size() + 99) / 100 - 1;
    auto real_percentile_value = real_samples[real_sample_index];

    // until we hit the maximum threshold, the error should not exceed bin size
    if (real_percentile_value < kBinSize * kBinCount) {
      EXPECT_LE(abs(ar_binned_sampler_get_threshold(bs_, temp) - real_percentile_value), kBinSize)
          << "percentile value: " << temp
          << ", threshold value: " << ar_binned_sampler_get_threshold(bs_, temp)
          << ", real value: " << real_percentile_value;
    } else {
      /**
       * Once we are over the maximum bin size, the error should not exceed
       * max_value - max_bin
       */
      EXPECT_LE(
          abs(ar_binned_sampler_get_threshold(bs_, temp) - real_percentile_value),
          real_samples.back() - thresholds.back())
          << "percentile value: " << temp
          << ", threshold value: " << ar_binned_sampler_get_threshold(bs_, temp)
          << ", real value: " << real_percentile_value;
    }

    // we also expect threshold values to be non-decreasing
    EXPECT_GE(ar_binned_sampler_get_threshold(bs_, temp), prev_threshold_value)
        << "percentile value: " << temp;
    prev_threshold_value = ar_binned_sampler_get_threshold(bs_, temp);
  }

  // percentile 100 will be max value
  EXPECT_EQ(ar_binned_sampler_get_threshold(bs_, 100), ar_binned_sampler_get_max(bs_));
}
