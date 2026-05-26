#pragma once

#include "comm_common.hh"
#include "comm_strap.hh"
#include "fft_hr_spo2.h"
#include "freertos/projdefs.h"
#include "max30102.hh"
#include "portmacro.h"
#include <array>
#include <cstdint>
#include <esp_log.h>
#include <optional>
#include <string.h>
#include <utility>

using std::array;
using std::optional;
using std::pair;

struct Sampler {
private:
  static constexpr const char *TAG = "HR-SpO2";

  static constexpr const uint32_t BUF_SIZE = 128;
  static constexpr const uint32_t SAMPLE_RATE = 25;
  static constexpr const uint32_t SKIP_SAMPLES_AFTER_SLEEP = 2;

  static constexpr const float CUTOFF_BPM_LOW = 25;
  static constexpr const float DECAY_BPS_HIGH = 0.6;
  static constexpr const uint32_t MIN_RED_SAMPLE_VALUE = 20000;
  static constexpr const uint32_t MIN_IR_SAMPLE_VALUE = 20000;

  static constexpr const uint32_t HISTORY_SIZE = 12;
  static constexpr const uint32_t HISTORY_MIN_KNOWN_SAMPLES = 9;

  static constexpr const float SUSPICIOUS_NEW_SAMPLE_MAD_THRESHOLD = 2.0;
  static constexpr const uint8_t SUSPICIOUS_NEW_SAMPLE_MAD_MIN_HR_THRESHOLD =
      10;
  static constexpr const uint8_t SUSPICIOUS_NEW_SAMPLE_MAD_MIN_SPO2_THRESHOLD =
      2;

  static constexpr const uint8_t SUSPICIOUS_HISTORY_MAD_HR_THRESHOLD = 20;
  static constexpr const uint8_t SUSPICIOUS_HISTORY_MAD_SPO2_THRESHOLD = 5;

  static constexpr const uint8_t SUSPICIOUS_HIGH_HR_THRESHOLD = 121;
  // 40 BPM is the minimum average sleeping heart rate:
  // https://www.sleepfoundation.org/physical-health/sleeping-heart-rate
  static constexpr const uint8_t SUSPICIOUS_LOW_HR_THRESHOLD = 39;

  // https://www.vinmec.com/eng/blog/what-is-the-spo2-index-in-a-normal-person-en
  static constexpr const uint8_t SUSPICIOUS_LOW_SPO2_THRESHOLD = 93;

  static constexpr const uint32_t NORMAL_CHECK_FREQUENCY_S = 10;
  static constexpr const uint32_t REDUCED_CHECK_FREQUENCY_S = 60;

  static constexpr const int32_t NORMAL_CHECK_FREQUENCY_DELAY_US =
      1'000'000 * NORMAL_CHECK_FREQUENCY_S - 1'000'000 * BUF_SIZE / SAMPLE_RATE;
  static constexpr const int32_t REDUCED_CHECK_FREQUENCY_DELAY_US =
      1'000'000 * REDUCED_CHECK_FREQUENCY_S -
      1'000'000 * BUF_SIZE / SAMPLE_RATE;
  static constexpr const uint32_t US_PER_SAMPLE = 1'000'000 / SAMPLE_RATE;

  MAX30102 &sensor;

  esp_pm_lock_handle_t cpu_freq_lock = nullptr;

  array<uint32_t, BUF_SIZE> red_samples, ir_samples;

public:
  Sampler(MAX30102 &sensor) : sensor(sensor) {
    red_samples.fill(0);
    ir_samples.fill(0);

    if (esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX, 0, "CPU max frequency",
                           &cpu_freq_lock) != ESP_OK) {
      cpu_freq_lock = nullptr;
    };
  }

private:
  inline pair<optional<uint8_t>, optional<uint8_t>> calc_hr_spo2() {
    for (size_t i = 0; i < BUF_SIZE; i++) {
      if (red_samples[i] < MIN_RED_SAMPLE_VALUE ||
          ir_samples[i] < MIN_IR_SAMPLE_VALUE) {
        // The sensor probably isn't next to skin
        return {};
      }
    }
    const auto [hr, spo2] =
        calc_hr_spo2_fft(red_samples.data(), ir_samples.data(), BUF_SIZE,
                         SAMPLE_RATE, CUTOFF_BPM_LOW, DECAY_BPS_HIGH);
    return {
        hr > 254 ? 254 : hr,
        spo2 > 100 ? 100 : spo2,
    };
  }

  inline void print_hr_spo2(optional<uint8_t> hr, optional<uint8_t> spo2) {
#if ESP_LOG_ENABLED(ESP_LOG_LEVEL_INFO)
    char hr_buf[11], spo2_buf[11];
    if (hr.has_value()) {
      snprintf(hr_buf, sizeof(hr_buf), "%3" PRIu8, hr.value());
    } else {
      strncpy(hr_buf, "N/A", sizeof(hr_buf));
    }
    if (spo2.has_value()) {
      snprintf(spo2_buf, sizeof(spo2_buf), "%3" PRIu8, spo2.value());
    } else {
      strncpy(spo2_buf, "N/A", sizeof(spo2_buf));
    }
    ESP_LOGI(TAG, "HR = %s, SpO2 = %s", hr_buf, spo2_buf);
#endif
  }

  static inline uint8_t median(const uint8_t *arr, size_t len) {
    return (len & 1)
               ? arr[len / 2]
               : ((uint16_t)arr[len / 2 - 1] + (uint16_t)arr[len / 2]) / 2;
  }

  inline bool
  can_lower_check_frequency(array<CommPatientData, HISTORY_SIZE> history,
                            uint8_t history_len, optional<uint8_t> hr,
                            optional<uint8_t> spo2) {
    if (!hr.has_value() || !spo2.has_value() || history_len < HISTORY_SIZE ||
        hr.value() <= SUSPICIOUS_LOW_HR_THRESHOLD ||
        hr.value() >= SUSPICIOUS_HIGH_HR_THRESHOLD ||
        spo2.value() <= SUSPICIOUS_LOW_SPO2_THRESHOLD) {
      ESP_LOGI(TAG, "Lower frequency: false %u %u %u %u %u %u", __LINE__,
               hr.has_value(), spo2.has_value(), history_len, hr.value_or(255),
               spo2.value_or(255));
      return false;
    }

    std::array<uint8_t, HISTORY_SIZE> hr_sorted, spo2_sorted;
    uint8_t hr_sorted_len = 0, spo2_sorted_len = 0;
    for (size_t i = 0; i < HISTORY_SIZE; i++) {
      if (history[i].heart_rate.has_value()) {
        hr_sorted[hr_sorted_len++] = history[i].heart_rate.value();
      }
      if (history[i].spo2.has_value()) {
        spo2_sorted[spo2_sorted_len++] = history[i].spo2.value();
      }
    }

    if (hr_sorted_len < HISTORY_MIN_KNOWN_SAMPLES ||
        spo2_sorted_len < HISTORY_MIN_KNOWN_SAMPLES) {
      ESP_LOGI(TAG, "Lower frequency: false %u %u %u", __LINE__, hr_sorted_len,
               spo2_sorted_len);
      return false;
    }

    const auto compare_fn = [](const void *a, const void *b) {
      return *(const uint8_t *)a - *(const uint8_t *)b;
    };
    qsort(hr_sorted.data(), hr_sorted_len, sizeof(uint8_t), compare_fn);
    qsort(spo2_sorted.data(), spo2_sorted_len, sizeof(uint8_t), compare_fn);

    const uint8_t hr_median = median(hr_sorted.data(), hr_sorted_len);
    const uint8_t spo2_median = median(spo2_sorted.data(), spo2_sorted_len);

    for (size_t i = 0; i < hr_sorted_len; i++) {
      const uint8_t value = hr_sorted[i];
      hr_sorted[i] = value < hr_median ? hr_median - value : value - hr_median;
    }
    for (size_t i = 0; i < spo2_sorted_len; i++) {
      const uint8_t value = spo2_sorted[i];
      spo2_sorted[i] =
          value < spo2_median ? spo2_median - value : value - spo2_median;
    }

    qsort(hr_sorted.data(), hr_sorted_len, sizeof(uint8_t), compare_fn);
    qsort(spo2_sorted.data(), spo2_sorted_len, sizeof(uint8_t), compare_fn);

    const uint8_t hr_mad =
        std::max(median(hr_sorted.data(), hr_sorted_len), (uint8_t)1);
    const uint8_t spo2_mad =
        std::max(median(spo2_sorted.data(), spo2_sorted_len), (uint8_t)1);

    const auto is_above_mad_threshold = [](const int16_t value,
                                           const int16_t median,
                                           const uint8_t mad,
                                           const uint8_t min_threshold) {
      const int16_t threshold =
          std::max((uint16_t)(SUSPICIOUS_NEW_SAMPLE_MAD_THRESHOLD * (float)mad),
                   (uint16_t)min_threshold);
      return value >= median + threshold || value <= median - threshold;
    };
    if (is_above_mad_threshold(hr.value(), hr_median, hr_mad,
                               SUSPICIOUS_NEW_SAMPLE_MAD_MIN_HR_THRESHOLD) ||
        is_above_mad_threshold(spo2.value(), spo2_median, spo2_mad,
                               SUSPICIOUS_NEW_SAMPLE_MAD_MIN_SPO2_THRESHOLD)) {
      ESP_LOGI(TAG, "Lower frequency: false %u %u %u %u %u %u %u", __LINE__,
               hr.value(), hr_median, hr_mad, spo2.value(), spo2_median,
               spo2_mad);
      return false;
    }

    if (hr_mad >= SUSPICIOUS_HISTORY_MAD_HR_THRESHOLD ||
        spo2_mad >= SUSPICIOUS_HISTORY_MAD_SPO2_THRESHOLD) {
      ESP_LOGI(TAG, "Lower frequency: false %u %u %u", __LINE__, hr_mad,
               spo2_mad);
      return false;
    }

    ESP_LOGI(TAG, "Lower frequency: true %u", __LINE__);
    return true;
  }

public:
  inline void run() {
    sensor.set_spo2();

    size_t filled = 0;
    size_t skip_samples = SKIP_SAMPLES_AFTER_SLEEP;

    std::array<CommPatientData, HISTORY_SIZE> history;
    uint8_t history_len = 0;

    for (;;) {
      sensor.wait_for_samples();

      TickType_t start_timestamp = xTaskGetTickCount();

      const auto samples = sensor.read_spo2_sample();
      if (skip_samples > 0) {
        skip_samples--;
        continue;
      }
      red_samples[filled] = samples[0];
      ir_samples[filled] = samples[1];
      filled++;
      if (filled >= BUF_SIZE) {
        if (cpu_freq_lock) {
          ESP_ERROR_CHECK(esp_pm_lock_acquire(cpu_freq_lock));
        }

        const auto [hr, spo2] = calc_hr_spo2();
        print_hr_spo2(hr, spo2);

        bool can_lower_check_frequency =
            this->can_lower_check_frequency(history, history_len, hr, spo2);

        if (history_len >= history.size()) {
          memmove(history.data(), history.data() + 1,
                  (history.size() - 1) * sizeof(decltype(history)::value_type));
          history_len = history.size() - 1;
        }
        history[history_len++] =
            CommPatientData{.heart_rate = hr, .spo2 = spo2};

        if (cpu_freq_lock) {
          ESP_ERROR_CHECK(esp_pm_lock_release(cpu_freq_lock));
        }

        CommStrap::get().set_heart_rate(hr);
        CommStrap::get().set_spo2(spo2);

        int32_t delay_us = can_lower_check_frequency
                               ? REDUCED_CHECK_FREQUENCY_DELAY_US
                               : NORMAL_CHECK_FREQUENCY_DELAY_US;
        while (delay_us >= US_PER_SAMPLE && sensor.sample_is_ready()) {
          sensor.read_spo2_sample();
          delay_us -= US_PER_SAMPLE;
        }
        if (delay_us >= std::max(SKIP_SAMPLES_AFTER_SLEEP * US_PER_SAMPLE,
                                 1000 * pdTICKS_TO_MS(1))) {
          sensor.set_sleep();
          vTaskDelayUntil(
              &start_timestamp,
              std::max(
                  pdMS_TO_TICKS((delay_us - (SKIP_SAMPLES_AFTER_SLEEP - 1) *
                                                US_PER_SAMPLE) /
                                1000),
                  (TickType_t)1));
          sensor.set_spo2();
          skip_samples = SKIP_SAMPLES_AFTER_SLEEP;
        }

        filled = 0;
      }
    }
  }
};
