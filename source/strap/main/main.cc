#include "comm_strap.hh"
#include "fft_hr_spo2.h"
#include "i2c.hh"
#include "max30102.hh"
#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <optional>
#include <utility>

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

#define TASK(name, ...)                                                        \
  void name(void *args) { __VA_ARGS__ vTaskDelete(nullptr); }

static TaskHandle_t process_samples_task_handle;

constexpr size_t BUF_SIZE = 128;
static uint32_t red_samples[BUF_SIZE], ir_samples[BUF_SIZE];

constexpr const uint8_t SAMPLE_RATE = 25;
constexpr const uint8_t CUTOFF_BPM_LOW = 20;
constexpr const uint8_t DECAY_BPS_HIGH = 5;

static std::pair<std::optional<uint8_t>, std::optional<uint8_t>>
calc_hr_spo2() {
  const auto [hr, spo2] =
      calc_hr_spo2_fft(red_samples, ir_samples, BUF_SIZE, SAMPLE_RATE,
                       CUTOFF_BPM_LOW, DECAY_BPS_HIGH);
  return {
      hr > 254 ? 254 : hr,
      spo2 > 100 ? 100 : spo2,
  };
}

constexpr uint8_t ROOM = 0;
constexpr uint8_t BED = 0;

TASK(process_samples, {
  I2CMasterBus bus(I2cMasterBusConfig{
      .sda = GPIO_NUM_1,
      .scl = GPIO_NUM_2,
      .clock_source = I2C_CLK_SRC_DEFAULT,
  });

  MAX30102 sensor(bus.handle);
  sensor.transfer_timeout_ms = 1000;
  sensor.start();
  sensor.set_spo2();

  memset(red_samples, 0, BUF_SIZE * 4);
  memset(ir_samples, 0, BUF_SIZE * 4);
  size_t filled = 0;

  for (;;) {
    while (!sensor.sample_is_ready())
      ;
    const auto samples = sensor.read_spo2_sample();
    if (filled == BUF_SIZE) {
      memmove(red_samples, red_samples + 1, (BUF_SIZE - 1) * 4);
      memmove(ir_samples, ir_samples + 1, (BUF_SIZE - 1) * 4);
      filled--;
    }
    red_samples[filled] = samples[0];
    ir_samples[filled] = samples[1];
    filled++;
    if (filled == BUF_SIZE) {
      filled = 0;
      const auto [hr, spo2] = calc_hr_spo2();
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
      ESP_LOGI("HR-SpO2", "HR = %s, SpO2 = %s", hr_buf, spo2_buf);
      CommStrap::get().set_heart_rate(hr);
      CommStrap::get().set_spo2(spo2);
    }
  }
});

extern "C" void app_main(void) {
  CommStrap::init(ROOM, BED);

  PD_ERROR_CHECK(xTaskCreate(process_samples, "process_samples", 8192, nullptr,
                             5, &process_samples_task_handle));
}
