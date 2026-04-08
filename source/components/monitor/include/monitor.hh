#pragma once

#include <array>
#include <optional>
#include <cstddef>
#include <cstdint>
#include <driver/i2c_master.h>
#include <esp_lcd_types.h>
#include <soc/gpio_num.h>

struct PatientData {
  std::optional<uint8_t> heart_rate;
  std::optional<uint8_t> spo2;
  bool is_warning;
};

struct MonitorPinConfig {
  gpio_num_t rst, power;

  static MonitorPinConfig DEFAULT;
};

inline MonitorPinConfig MonitorPinConfig::DEFAULT = {
    .rst = GPIO_NUM_21,
    .power = GPIO_NUM_36,
};

struct Monitor {
  static constexpr size_t WIDTH = 128;
  static constexpr size_t HEIGHT = 64;

  static constexpr uint8_t DEFAULT_I2C_ADDR = 0x3C;

  i2c_master_dev_handle_t dev;
  int transfer_timeout_ms = 100;

  Monitor(i2c_master_bus_handle_t bus, uint8_t i2c_addr = DEFAULT_I2C_ADDR,
          MonitorPinConfig pin_config = MonitorPinConfig::DEFAULT);

  void clear(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
  inline void clear() { clear(0, 0, WIDTH - 1, HEIGHT - 1); }
  void flush(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
  inline void flush() { flush(0, 0, WIDTH - 1, HEIGHT - 1); }

  void invert(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

  void square(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
  void square_dotted(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);
  void number(uint8_t x1, uint8_t y1, uint32_t value);
  void text(uint8_t x1, uint8_t y1, const char* value);

  void draw_patient_data(const std::array<std::optional<PatientData>, 4>& data);

  inline ~Monitor() { i2c_master_bus_rm_device(dev); }

private:
  std::array<uint8_t, WIDTH * HEIGHT / 8> bitmap;
  std::array<uint8_t, WIDTH * HEIGHT / 8> bitmap_translated;

  esp_lcd_panel_handle_t panel_handle;

  MonitorPinConfig pin_config;
};
