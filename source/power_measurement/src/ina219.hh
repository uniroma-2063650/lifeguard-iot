#pragma once

#include "ina219_regs.hh"
#include "log.hh"
#include "twi.hh"
#include <stdlib.h>
#include <util/delay.h>

namespace ina219 {

namespace regs = ina219_regs;

constexpr const char *TAG = "INA219";

constexpr double BUS_VOLTAGE_LSB = 0.004;
constexpr double SHUNT_VOLTAGE_LSB = 0.00001;

constexpr double MAX_BUS_VOLTAGE = 3.7;
constexpr double MAX_SHUNT_VOLTAGE = 0.32;
constexpr double R_SHUNT = 0.1;

constexpr double MAX_POSSIBLE_CURRENT = (MAX_SHUNT_VOLTAGE / R_SHUNT);

constexpr double MAX_EXPECTED_CURRENT = 2.4;

constexpr double MIN_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 32767.0);
constexpr double MAX_CURRENT_LSB = (MAX_EXPECTED_CURRENT / 4096.0);

constexpr double CURRENT_LSB = 0.000075;
static_assert(CURRENT_LSB >= MIN_CURRENT_LSB);
static_assert(CURRENT_LSB <= MAX_CURRENT_LSB);

constexpr double POWER_LSB = (5000.0 * BUS_VOLTAGE_LSB * CURRENT_LSB);

constexpr uint16_t CAL_VALUE = 0.04096 / (CURRENT_LSB * R_SHUNT);

constexpr double MAX_CURRENT_BEFORE_OVF = (CURRENT_LSB * 32767.0);
constexpr double MAX_SHUNT_VOLTAGE_BEFORE_OVF =
    (MAX_CURRENT_BEFORE_OVF * R_SHUNT);
constexpr double MAX_POWER_BEFORE_OVF =
    (MAX_CURRENT_BEFORE_OVF * MAX_BUS_VOLTAGE);

constexpr uint8_t DEV_ADDR = 0x40;

#define TWI_ERROR_CHECK(x)                                                     \
  do {                                                                         \
    uint8_t result = (x);                                                      \
    if (result) {                                                              \
      LOGI(TAG, "TWI failed with status %02X", result);                        \
      while (1)                                                                \
        ;                                                                      \
    }                                                                          \
  } while (0);

inline uint16_t read_reg(uint8_t reg_addr) {
  LOGV(TAG, "Reading from %02X", reg_addr);
  const uint8_t write_data[]{reg_addr};
  uint8_t read_data[2]{0, 0};
  TWI_ERROR_CHECK(twi::twi_read_write(DEV_ADDR, write_data, sizeof(write_data),
                                      read_data, sizeof(read_data)));
  const uint16_t result = ((uint16_t)read_data[0] << 8) | read_data[1];
  LOGV(TAG, "Read from %02X: %04X", reg_addr, result);
  return result;
}

inline uint8_t write_reg_checked(uint8_t reg_addr, uint16_t value) {
  LOGV(TAG, "Writing %04X to %02X", value, reg_addr);
  const uint8_t write_data[]{reg_addr, (uint8_t)(value >> 8), (uint8_t)value};
  return twi::twi_write(DEV_ADDR, write_data, sizeof(write_data));
  LOGV(TAG, "Written");
}

inline void write_reg(uint8_t reg_addr, uint16_t value) {
  TWI_ERROR_CHECK(write_reg_checked(reg_addr, value));
}

inline void reset() {
  LOGI(TAG, "Resetting...");
  for (int i = 0; i < 2; i++) {
    uint8_t result = write_reg_checked(regs::CONFIGURATION,
                                       regs::Configuration{
                                           .mode = 0,
                                           .shunt_adc_mode = 0,
                                           .bus_adc_mode = 0,
                                           .pga_gain = 0,
                                           .bus_voltage_range = 0,
                                           .reset = true,
                                       }
                                           .raw_value);
    if (result == 0) {
      break;
    } else {
      LOGI(TAG, "Failed to reset: %02X", result);
      if (i > 0) {
        while (1)
          ;
      }
    }
  }
  while (regs::Configuration{.raw_value = read_reg(regs::CONFIGURATION)}.reset)
    ;
  LOGI(TAG, "Reset");
  write_reg(regs::CALIBRATION, CAL_VALUE);
  write_reg(
      regs::CONFIGURATION,
      regs::Configuration{
          .mode = 0b101,            // Shunt only
          .shunt_adc_mode = 0b1111, // 128 12-bit sample average, every 68.1 ms
          .bus_adc_mode = 0b1000,   // Unused
          .pga_gain = 3,            // ±320mV mV shunt voltage FSR
          .bus_voltage_range = 0,   // 16 V bus voltage FSR (powered at 3.3 V)
          .reset = false,
      }
          .raw_value);
}

void wait_conversion_ready() {
  for (;;) {
    regs::BusVoltage bus_voltage{.raw_value = read_reg(regs::BUS_VOLTAGE)};
    if (bus_voltage.conversion_ready)
      break;
    _delay_ms(10);
  }
}

double read_bus_voltage() {
  return (double)regs::BusVoltage{.raw_value = read_reg(regs::BUS_VOLTAGE)}
             .voltage *
         BUS_VOLTAGE_LSB;
}

double read_shunt_voltage() {
  return (double)(int16_t)read_reg(regs::SHUNT_VOLTAGE) * SHUNT_VOLTAGE_LSB * 1000;
}

double read_current() {
  return (double)(int16_t)read_reg(regs::CURRENT) * CURRENT_LSB * 1000;
}

double read_power() {
  return (double)(int16_t)read_reg(regs::POWER) * POWER_LSB * 1000;
}

} // namespace ina219
