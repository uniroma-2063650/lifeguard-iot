#include <cstdio>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <optional>
#include <utility>

#include "comm_strap.hh"
#include "i2c.hh"
#include "max30102.hh"
#include "maxim_hr_spo2.h"

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

#define TASK(name, ...)                                                        \
  void name(void *args) { __VA_ARGS__ vTaskDelete(nullptr); }

static TaskHandle_t process_samples_task_handle;

constexpr size_t BUF_SIZE = MAXIM_HR_SPO2_BUFFER_SIZE;
static uint32_t red_samples[BUF_SIZE], ir_samples[BUF_SIZE];

static std::pair<std::optional<uint8_t>, std::optional<uint8_t>>
calc_hr_spo2() {
  int32_t spo2, hr;
  int8_t spo2_valid, hr_valid;
  maxim_heart_rate_and_oxygen_saturation(ir_samples, BUF_SIZE, red_samples,
                                         &spo2, &spo2_valid, &hr, &hr_valid);
  return {
      hr_valid ? (hr > 254 ? 254
                  : hr < 0 ? 0
                           : hr)
               : std::optional<uint8_t>{},
      spo2_valid ? (spo2 > 100 ? 100
                    : spo2 < 0 ? 0
                               : spo2)
                 : std::optional<uint8_t>{},
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
      CommStrap::get().sendPacket({.room = ROOM,
                                   .bed = BED,
                                   .kind = CommPacketKind::HEART_DATA,
                                   .heart_data = {.hr = hr, .spo2 = spo2}});
    }
  }
});

extern "C" void app_main(void) {
  CommStrap::init(true, 0);

  PD_ERROR_CHECK(xTaskCreate(process_samples, "process_samples", 8192, nullptr,
                             5, &process_samples_task_handle));
}
