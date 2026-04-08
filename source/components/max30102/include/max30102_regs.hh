#pragma once

#include <cstdint>

namespace max30102_regs {

union InterruptMask1 {
  struct {
    bool power_ready : 1;
    bool : 4;
    bool ambient_light_cancellation_overflow : 1;
    bool fifo_data_ready : 1;
    bool fifo_almost_full : 1;
  };
  uint8_t raw_value;
};
union InterruptMask2 {
  struct {
    bool : 1;
    bool die_temp_ready : 1;
    bool : 6;
  };
  uint8_t raw_value;
};

constexpr uint8_t INTERRUPT_STATUS_1 = 0x00; // InterruptMask1
constexpr uint8_t INTERRUPT_STATUS_2 = 0x01; // InterruptMask2
constexpr uint8_t INTERRUPT_ENABLE_1 = 0x02; // InterruptMask1
constexpr uint8_t INTERRUPT_ENABLE_2 = 0x03; // InterruptMask2

constexpr uint8_t FIFO_WRITE_POINTER = 0x04; // uint5
constexpr uint8_t OVERFLOW_COUNTER   = 0x05; // uint5
constexpr uint8_t FIFO_READ_POINTER  = 0x06; // uint5
constexpr uint8_t FIFO_DATA          = 0x07; // uint8

union FifoConfig {
  struct {
    /*
    0x0 = FIFO almost full at 32 unread samples
    ... (decrement by 1)
    0xF = FIFO almost full at 17 unread samples
    */
    uint8_t fifo_almost_full_free_limit : 4;
    bool fifo_rolls_on_full : 1;
    /*
    0b000 = 1-sample averaging (no averaging)
    ... (multiply by 2 each step)
    0b101 and above = 32-sample averaging
    */
    uint8_t sample_averaging : 3;
  };
  uint8_t raw_value;
};
constexpr uint8_t FIFO_CONFIG = 0x08;

enum class Mode {
  HEART_RATE = 0b010, // Red
  SPO2       = 0b011, // Red + IR
  MULTI_LED  = 0b111, // Red + IR
};
union ModeConfig {
  struct {
    Mode mode : 3;
    bool : 3;
    bool reset : 1;
    // Enables power-saving mode (no new data collected and no interrupts, all
    // registers stay readable and writable)
    bool shutdown : 1;
  };
  uint8_t raw_value;
};
constexpr uint8_t MODE_CONFIG = 0x09;

union SpO2Config {
  struct {
    /*
    0b00 =  68.95 µs, 15-bit ADC
    0b01 = 117.78 µs, 16-bit ADC
    0b02 = 215.44 µs, 17-bit ADC
    0b03 = 419.75 µs, 18-bit ADC
    */
    uint8_t led_pulse_width : 2;
    /*
    0b000 = 50 samples/s
    0b001 = 100 samples/s
    0b010 = 200 samples/s
    0b011 = 400 samples/s
    0b100 = 800 samples/s
    0b101 = 1000 samples/s
    0b110 = 1600 samples/s
    0b111 = 3200 samples/s
    */
    uint8_t spo2_sample_rate : 3;
    /*
    0b00 =  7.81 pA LSB,  2048 nA full range
    0b01 = 15.63 pA LSB,  4096 nA full range
    0b10 = 31.25 pA LSB,  8192 nA full range
    0b11 = 62.50 pA LSB, 16384 nA full range
    NOTE: This changes the full 18-bit range
    */
    uint8_t spo2_adc_range : 2;
    bool : 1;
  };
  uint8_t raw_value;
};
constexpr uint8_t SPO2_CONFIG = 0x0A;

/*
0x00 = 0 mA
... (increment by 0.2 mA)
0xFF = 51.0 mA
*/
constexpr uint8_t LED_PULSE_AMPLITUDE_RED = 0x0C; // uint8
constexpr uint8_t LED_PULSE_AMPLITUDE_IR  = 0x0D; // uint8

enum class LedAssignment {
  NONE = 0b000,
  RED  = 0b001,
  IR   = 0b010,
  NONE_0b011 = 0b011,
  NONE_0b100 = 0b100,
};
union MultiLedModeControl {
  struct {
    LedAssignment first : 3;
    bool : 1;
    LedAssignment second : 3;
    bool : 1;
  };
  uint8_t raw_value;
};
// Each sample produces output from up to 4 slots
constexpr uint8_t MULTI_LED_MODE_CONTROL_1 = 0x11; // MultiLedModeControl
constexpr uint8_t MULTI_LED_MODE_CONTROL_2 = 0x12; // MultiLedModeControl

// Increments of 1 °C
constexpr uint8_t DIE_TEMP_INTEGER  = 0x1F; // int8
// Increments of 0.0625 °C
constexpr uint8_t DIE_TEMP_FRACTION = 0x20; // uint4

union DieTempConfig {
  struct {
    // Starts a temperature reading with the next I2C transfer, then self-clears
    bool temperature_enable : 1;
    bool : 7;
  };
  uint8_t raw_value;
};
constexpr uint8_t DIE_TEMP_CONFIG   = 0x21;

constexpr uint8_t REVISION_ID = 0xFE;
constexpr uint8_t PART_ID = 0xFF;

} // namespace max30102_regs
