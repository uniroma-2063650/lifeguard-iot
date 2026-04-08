#include "max30102.hh"
#include "max30102_regs.hh"
#include <array>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

constexpr const char *TAG = "MAX30102";

using std::array;
namespace regs = max30102_regs;

MAX30102::MAX30102(i2c_master_bus_handle_t bus, uint8_t i2c_addr,
                   MAX30102PinConfig pin_config)
    : pin_config(pin_config) {
  ESP_ERROR_CHECK(gpio_reset_pin(pin_config.power));
  const gpio_config_t gpio_config_value = {
      .pin_bit_mask = ((uint64_t)1 << pin_config.power),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&gpio_config_value));
  ESP_ERROR_CHECK(gpio_set_level(pin_config.power, 1));
  ESP_LOGI(TAG, "Initialized GPIO");

  const i2c_device_config_t dev_config = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = i2c_addr,
      .scl_speed_hz = 400000,
      .scl_wait_us = 0,
      .flags{.disable_ack_check = false},
  };
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_config, &this->dev));
  ESP_LOGI(TAG, "Initialized I2C");
}

void MAX30102::reset() {
  ESP_LOGI(TAG, "Resetting...");
  for (int i = 0; i < 2; i++) {
    esp_err_t result = write_reg_checked(regs::MODE_CONFIG,
                                         regs::ModeConfig{
                                             .mode = regs::Mode::HEART_RATE,
                                             .reset = true,
                                             .shutdown = false,
                                         }
                                             .raw_value);
    if (result == ESP_ERR_INVALID_STATE && i == 0) {
      continue;
    } else if (result == ESP_OK) {
      break;
    } else {
      ESP_ERROR_CHECK(result);
    }
  }
  while (regs::ModeConfig{.raw_value = read_reg(regs::MODE_CONFIG)}.reset) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_LOGI(TAG, "Reset");
}

void MAX30102::start() {
  reset();
  write_reg(regs::FIFO_CONFIG,
            regs::FifoConfig{
                .fifo_almost_full_free_limit = 8,
                .fifo_rolls_on_full = true,
                .sample_averaging = 0b100, // 16-sample averaging
            }
                .raw_value);
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::HEART_RATE,
                .reset = false,
                .shutdown = false,
            }
                .raw_value);
  write_reg(regs::SPO2_CONFIG,
            regs::SpO2Config{
                .led_pulse_width = 0b11,   // 419.75 µs, 18-bit ADC
                .spo2_sample_rate = 0b011, // 400 samples/s
                .spo2_adc_range = 0b11,    // 62.50 pA LSB, 16384 nA full range
            }
                .raw_value);
  write_reg(regs::LED_PULSE_AMPLITUDE_RED, 0x80); // 25.6 mA
  write_reg(regs::LED_PULSE_AMPLITUDE_IR, 0x80);  // 25.6 mA
  ESP_LOGI(TAG, "Configured");
  ESP_LOGI(TAG, "%02X: %02X", regs::FIFO_CONFIG, read_reg(regs::FIFO_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::MODE_CONFIG, read_reg(regs::MODE_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::SPO2_CONFIG, read_reg(regs::SPO2_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::LED_PULSE_AMPLITUDE_RED,
           read_reg(regs::LED_PULSE_AMPLITUDE_RED));
  ESP_LOGI(TAG, "%02X: %02X", regs::LED_PULSE_AMPLITUDE_IR,
           read_reg(regs::LED_PULSE_AMPLITUDE_IR));
}

void MAX30102::set_hr() {
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::HEART_RATE,
                .reset = false,
                .shutdown = false,
            }
                .raw_value);
  ESP_LOGI(TAG, "Configured");
}

void MAX30102::set_spo2() {
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::SPO2,
                .reset = false,
                .shutdown = false,
            }
                .raw_value);
  ESP_LOGI(TAG, "Configured");
}

bool MAX30102::sample_is_ready() {
  return read_reg(regs::FIFO_READ_POINTER) !=
         read_reg(regs::FIFO_WRITE_POINTER);
}

array<uint32_t, 1> MAX30102::read_hr_sample() {
  return read_sample_channels<1>();
}

array<uint32_t, 2> MAX30102::read_spo2_sample() {
  return read_sample_channels<2>();
}

uint8_t MAX30102::read_reg(const uint8_t reg_addr) {
  ESP_LOGD(TAG, "Reading from %02X", reg_addr);
  const array<uint8_t, 1> write_data{reg_addr};
  array<uint8_t, 1> read_data{};
  ESP_ERROR_CHECK(i2c_master_transmit_receive(
      this->dev, write_data.data(), write_data.size(), read_data.data(),
      read_data.size(), this->transfer_timeout_ms));
  return read_data[0];
}

esp_err_t MAX30102::write_reg_checked(const uint8_t reg_addr,
                                      const uint8_t data) {
  ESP_LOGI(TAG, "Writing %02X to %02X", data, reg_addr);
  const array<uint8_t, 2> write_data{reg_addr, data};
  return i2c_master_transmit(this->dev, write_data.data(), write_data.size(),
                             this->transfer_timeout_ms);
}

void MAX30102::write_reg(const uint8_t reg_addr, const uint8_t data) {
  ESP_ERROR_CHECK(write_reg_checked(reg_addr, data));
}

template <uint8_t channels>
array<uint32_t, channels> MAX30102::read_sample_channels() {
  const array<uint8_t, 1> write_data{max30102_regs::FIFO_DATA};
  array<uint8_t, 3 * channels> read_data{};
  ESP_ERROR_CHECK(i2c_master_transmit_receive(
      this->dev, write_data.data(), write_data.size(), read_data.data(),
      read_data.size(), this->transfer_timeout_ms));
  array<uint32_t, channels> result;
  for (int i = 0; i < channels; i++) {
    uint8_t *sample_data = &read_data[i * 3];
    result[i] = ((uint32_t)(sample_data[0]) << 16) |
                ((uint32_t)(sample_data[1]) << 8) | (uint32_t)(sample_data[2]);
  }
  return result;
}
