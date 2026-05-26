#pragma once

#include <stdint.h>

namespace ina219_regs {

union Configuration {
  struct {
    uint8_t mode : 3;
    uint8_t shunt_adc_mode : 4;
    uint8_t bus_adc_mode : 4;
    uint8_t pga_gain : 2;
    bool bus_voltage_range : 1;
    bool : 1;
    bool reset : 1;
  };
  uint16_t raw_value;
};
union BusVoltage {
  struct {
    bool overflow : 1;
    bool conversion_ready : 1;
    bool : 1;
    uint16_t voltage : 13;
  };
  uint16_t raw_value;
};

constexpr uint8_t CONFIGURATION = 0x00; // Configuration
constexpr uint8_t SHUNT_VOLTAGE = 0x01; // int16
constexpr uint8_t BUS_VOLTAGE = 0x02;   // BusVoltage
constexpr uint8_t POWER = 0x03;         // uint16
constexpr uint8_t CURRENT = 0x04;       // int16
constexpr uint8_t CALIBRATION = 0x05;   // uint16

} // namespace ina219_regs
