#pragma once

#include <array>
#include <driver/i2c_master.h>
#include <soc/gpio_num.h>

using std::array;

struct MAX30102PinConfig {
  gpio_num_t power;

  static MAX30102PinConfig DEFAULT;
};

inline MAX30102PinConfig MAX30102PinConfig::DEFAULT = {
    .power = GPIO_NUM_4,
};

struct MAX30102 {
  static constexpr uint8_t DEFAULT_I2C_ADDR = 0x57;

  i2c_master_dev_handle_t dev;
  int transfer_timeout_ms = 100;

  MAX30102(i2c_master_bus_handle_t bus, uint8_t i2c_addr = DEFAULT_I2C_ADDR,
           MAX30102PinConfig pin_config = MAX30102PinConfig::DEFAULT);

  inline ~MAX30102() { i2c_master_bus_rm_device(dev); }

  void start();
  void set_hr();
  void set_spo2();
  bool sample_is_ready();
  array<uint32_t, 1> read_hr_sample();
  array<uint32_t, 2> read_spo2_sample();

private:
  MAX30102PinConfig pin_config;

  void reset();
  uint8_t read_reg(uint8_t reg_addr);
  esp_err_t write_reg_checked(uint8_t reg_addr, uint8_t value);
  void write_reg(uint8_t reg_addr, uint8_t value);
  template <uint8_t channels> array<uint32_t, channels> read_sample_channels();
};
