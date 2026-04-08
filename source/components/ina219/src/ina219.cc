#include "ina219.hh"

constexpr double VOLTAGE_LSB = 0.004;

constexpr double MAX_BUS_VOLTAGE = 3.7;
constexpr double MAX_SHUNT_VOLTAGE = 3.7;
constexpr double R_SHUNT = 0.1;

constexpr double MAX_POSSIBLE_CURRENT = (MAX_SHUNT_VOLTAGE / R_SHUNT);

constexpr double MAX_EXPECTED_CURRENT = 2;

constexpr double MIN_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 32767.0);
constexpr double MAX_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 4096.0);

constexpr double CURRENT_LSB = 0.000075;
static_assert(CURRENT_LSB >= MIN_CURRENT_LSB);
static_assert(CURRENT_LSB <= MAX_CURRENT_LSB);

constexpr double POWER_LSB = (5000.0 * VOLTAGE_LSB * CURRENT_LSB);

constexpr uint16_t CAL_VALUE = 0.04096 / (CURRENT_LSB * R_SHUNT);

constexpr double MAX_CURRENT_BEFORE_OVF = (CURRENT_LSB * 32767.0);
constexpr double MAX_SHUNT_VOLTAGE_BEFORE_OVF = (MAX_CURRENT_BEFORE_OVF * R_SHUNT);
constexpr double MAX_POWER_BEFORE_OVF = (MAX_CURRENT_BEFORE_OVF * MAX_BUS_VOLTAGE);

INA219::INA219(i2c_master_bus_handle_t bus, uint8_t i2c_addr) {
  const i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = i2c_addr,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags{.disable_ack_check = false},
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_config, &this->dev));
}
