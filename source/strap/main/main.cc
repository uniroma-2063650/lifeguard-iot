#include "comm_strap.hh"
#include "i2c.hh"
// #include "monitor.hh"
#include "sampler.hh"
#include <driver/gpio.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

#define TASK(name, ...)                                                        \
  void name(void *args) { __VA_ARGS__ vTaskDelete(nullptr); }

static TaskHandle_t process_samples_task_handle;

constexpr uint8_t ROOM = 0;
constexpr uint8_t BED = 0;

TASK(process_samples, {
  I2CMasterBus bus(I2cMasterBusConfig{
      .sda = GPIO_NUM_2,
      .scl = GPIO_NUM_4,
      .clock_source = I2C_CLK_SRC_DEFAULT,
      .port = I2C_NUM_0,
  });

  /* I2CMasterBus monitor_bus(I2cMasterBusConfig{
      .sda = GPIO_NUM_17,
      .scl = GPIO_NUM_18,
      .clock_source = I2C_CLK_SRC_DEFAULT,
      .port = I2C_NUM_1,
  });

  Monitor monitor(monitor_bus.handle);
#define MONITOR_STEP()                                                         \
  monitor.clear();                                                             \
  monitor.text(0, 0, "Line %u", __LINE__);                                     \
  monitor.flush(); */
#define MONITOR_STEP()

  MONITOR_STEP();
  MAX30102 sensor(bus.handle);
  MONITOR_STEP();
  sensor.transfer_timeout_ms = 1000;
  MONITOR_STEP();
  sensor.start();
  MONITOR_STEP();

  Sampler(sensor).run();

  vTaskDelete(nullptr);
});

extern "C" void app_main(void) {
  esp_pm_config_t pm_config = {
      .max_freq_mhz = 80, .min_freq_mhz = 10, .light_sleep_enable = true};
  ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

  CommStrap::init(ROOM, BED);

  ESP_ERROR_CHECK(esp_sleep_enable_gpio_wakeup());
  ESP_ERROR_CHECK(gpio_install_isr_service(0));

  PD_ERROR_CHECK(xTaskCreate(process_samples, "process_samples", 8192, nullptr,
                             5, &process_samples_task_handle));
}
