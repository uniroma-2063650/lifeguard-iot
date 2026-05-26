#include "fft_hr_spo2.h"
#include "arduinoFFT.h"
#include <cmath>
#include <cstring>
#include <esp_log.h>

constexpr const char *TAG = "HR-SpO2";

// Calculated as -45.060 * ratio^2 + 30.354 * ratio + 94.845
// Covers ratios between 0.00 and 1.825 excluded (2-decimal-digit fixed-point)
// The above formula was taken from Maxim's code
constexpr const uint8_t SPO2_TABLE[183] = {
    95,  95,  95,  96,  96,  96,  97,  97,  97,  97,  97,  98,  98,  98,  98,
    98,  99,  99,  99,  99,  99,  99,  99,  99,  100, 100, 100, 100, 100, 100,
    100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 99,
    99,  99,  99,  99,  99,  99,  99,  98,  98,  98,  98,  98,  98,  97,  97,
    97,  97,  96,  96,  96,  96,  95,  95,  95,  94,  94,  94,  93,  93,  93,
    92,  92,  92,  91,  91,  90,  90,  89,  89,  89,  88,  88,  87,  87,  86,
    86,  85,  85,  84,  84,  83,  82,  82,  81,  81,  80,  80,  79,  78,  78,
    77,  76,  76,  75,  74,  74,  73,  72,  72,  71,  70,  69,  69,  68,  67,
    66,  66,  65,  64,  63,  62,  62,  61,  60,  59,  58,  57,  56,  56,  55,
    54,  53,  52,  51,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,  40,
    39,  38,  37,  36,  35,  34,  33,  31,  30,  29,  28,  27,  26,  25,  23,
    22,  21,  20,  19,  17,  16,  15,  14,  12,  11,  10,  9,   7,   6,   5,
    3,   2,   1};

void apply_band_pass_filter(float *const fft_magnitudes, const size_t len,
                            const float cutoff_bpm_low,
                            const float decay_bps_high,
                            const float freq_scale) {
  const float rc_low = 1 / ((2.0 * M_PI / 60.0) * cutoff_bpm_low);
  for (size_t i = 0; i < len; i++) {
    const float frequency = i * freq_scale;
    float low_pass_scale = frequency * rc_low;
    low_pass_scale /= 1 + low_pass_scale;
    float high_pass_scale = std::pow(decay_bps_high, frequency);
    fft_magnitudes[i] *= low_pass_scale * high_pass_scale;
  }
}

std::pair<std::optional<uint32_t>, std::optional<uint32_t>>
calc_hr_spo2_fft(const uint32_t *const red_buffer,
                 const uint32_t *const ir_buffer, const size_t len,
                 const float sample_rate, const float cutoff_bpm_low,
                 const float decay_bps_high) {
  // for (size_t i = 0; i < len; i++) {
  //   ESP_LOGI(TAG, "%u %u", red_buffer[i], ir_buffer[i]);
  // }

  float *const real_buffer = new float[len], *const img_buffer = new float[len];
  for (size_t i = 0; i < len; i++) {
    real_buffer[i] = (float)(red_buffer[i] + ir_buffer[i]);
  }
  memset(img_buffer, 0, len * sizeof(float));

  // Find the heart rate by finding the peak frequency (damping lower
  // frequencies to minimize errors from i.e. moving around during measurement)
  ArduinoFFT<float> fft(real_buffer, img_buffer, len, sample_rate);
  fft.dcRemoval();
  fft.windowing(FFTWindow::Hamming, FFTDirection::Forward, false);
  fft.compute(FFTDirection::Forward);
  fft.complexToMagnitude();
  apply_band_pass_filter(real_buffer, (len >> 1) + 1, cutoff_bpm_low,
                         decay_bps_high, sample_rate / len);
  const float bps = fft.majorPeakParabola();
  delete[] real_buffer;
  delete[] img_buffer;
  if (bps == 0.0)
    return {};

  // Find the SpO2 by calculating the average ratio between IR and red channels
  // in valleys; the valleys are found by splitting the samples into "heartbeat
  // windows" based on the found heart rate
  std::optional<uint32_t> spo2{};
  const float heartbeat_window_size =
      std::min(sample_rate * 2 / bps, (float)len);
  ESP_LOGI(TAG, "Heartbeat window size: %f", heartbeat_window_size);
  if (heartbeat_window_size >= 1.0) {
    size_t found_ratios = 0;
    float ratio_sum = 0.0;

    for (float offset = 0.0, next_offset = offset + heartbeat_window_size;
         next_offset < len;
         offset = next_offset, next_offset += heartbeat_window_size) {
      const size_t i = offset;
      const size_t end_i = next_offset;

      uint32_t red_max = 0, ir_max = 0;
      for (size_t j = 0; j < end_i; j++) {
        red_max = std::max(red_max, red_buffer[j]);
        ir_max = std::max(ir_max, ir_buffer[j]);
      }

#define REL_RED(i)
#define REL_IR(i) ((int32_t)ir_buffer[min_i] - (int32_t)ir_max)

      size_t min_i = i;
      uint32_t min = red_buffer[i] + ir_buffer[i];
      for (size_t j = i + 1; j < end_i; j++) {
        const uint32_t value = red_buffer[j] + ir_buffer[j];
        if (value < min) {
          min_i = j;
          min = value;
        }
      }

      const uint32_t red_diff = red_max - red_buffer[min_i];
      const uint32_t ir_diff = ir_max - ir_buffer[min_i];
      ratio_sum +=
          ((float)red_diff / (float)red_max) / ((float)ir_diff / (float)ir_max);
      ESP_LOGI(TAG, "Ratio: %f",
               ((float)red_diff / (float)red_max) /
                   ((float)ir_diff / (float)ir_max));
      found_ratios++;
    }

    if (found_ratios >= 1) {
      const float ratio_avg = ratio_sum / found_ratios;
      ESP_LOGI(TAG, "Ratio avg: %f", ratio_avg);
      if (ratio_avg >= 0.0f && ratio_avg < 1.825f) {
        spo2 = SPO2_TABLE[std::min(
            (int)std::max(std::round(ratio_avg * 100.0f), 0.0f), 183)];
      }
    }
  }

  return {round(bps * 60.0), spo2};
}
