#pragma once

#include <cstdint>
#include <driver/i2c_master.h>

struct INA219 {
  static constexpr uint8_t DEFAULT_I2C_ADDR = 0x3C;

  i2c_master_dev_handle_t dev;
  int transfer_timeout_ms = 100;

  INA219(i2c_master_bus_handle_t bus, uint8_t i2c_addr = DEFAULT_I2C_ADDR);

  inline ~INA219() { i2c_master_bus_rm_device(dev); }
};
