#pragma once

#include <array>
#include <atomic>
#include <driver/i2c_master.h>
#include <esp_pm.h>
#include <freertos/FreeRTOS.h>
#include <soc/gpio_num.h>

using std::array;

struct MAX30102PinConfig {
  gpio_num_t interrupt;

  static MAX30102PinConfig DEFAULT;
};

inline MAX30102PinConfig MAX30102PinConfig::DEFAULT = {
    .interrupt = GPIO_NUM_1,
};

struct MAX30102 {
  static constexpr uint8_t DEFAULT_I2C_ADDR = 0x57;

  i2c_master_dev_handle_t dev;
  int transfer_timeout_ms = 100;

  MAX30102(i2c_master_bus_handle_t bus, uint8_t i2c_addr = DEFAULT_I2C_ADDR,
           MAX30102PinConfig pin_config = MAX30102PinConfig::DEFAULT);

  ~MAX30102();

  void start();
  void set_hr();
  void set_spo2();
  void set_sleep();
  bool sample_is_ready();
  void wait_for_samples();
  array<uint32_t, 1> read_hr_sample();
  array<uint32_t, 2> read_spo2_sample();

private:
  bool is_spo2;
  std::atomic_bool queue_len_needs_update;
  TaskHandle_t waiting_task = nullptr;
  size_t queue_len;
  MAX30102PinConfig pin_config;
  esp_pm_lock_handle_t sleep_lock;

  void reset();
  uint8_t read_reg(uint8_t reg_addr);
  esp_err_t write_reg_checked(uint8_t reg_addr, uint8_t value);
  void write_reg(uint8_t reg_addr, uint8_t value);
  template <uint8_t channels> array<uint32_t, channels> read_sample_channels();

  static void handle_almost_full_interrupt_static(void *arg) {
    ((MAX30102 *)arg)->handle_almost_full_interrupt();
  }
  void handle_almost_full_interrupt();
  void update_queue_len();
};
