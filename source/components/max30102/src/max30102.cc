#include "max30102.hh"
#include "esp_pm.h"
#include "max30102_regs.hh"
#include <array>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>
#include <freertos/task.h>

constexpr const char *TAG = "MAX30102";

using std::array;
namespace regs = max30102_regs;

constexpr const size_t QUEUE_ALMOST_FULL_THRESHOLD = 26;

MAX30102::MAX30102(i2c_master_bus_handle_t bus, uint8_t i2c_addr,
                   MAX30102PinConfig pin_config)
    : pin_config(pin_config) {
  ESP_ERROR_CHECK(gpio_reset_pin(pin_config.interrupt));
  const gpio_config_t gpio_config_value = {
      .pin_bit_mask = ((uint64_t)1 << pin_config.interrupt),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  ESP_ERROR_CHECK(gpio_config(&gpio_config_value));

  ESP_LOGI(TAG, "Initialized GPIO");

  if (esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0,
                         "MAX30102 I2C light sleep lock",
                         &sleep_lock) != ESP_OK) {
    sleep_lock = nullptr;
  }

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

MAX30102::~MAX30102() {
  i2c_master_bus_rm_device(dev);
  ESP_ERROR_CHECK(gpio_reset_pin(pin_config.interrupt));
  ESP_ERROR_CHECK(gpio_wakeup_disable(pin_config.interrupt));
  if (sleep_lock) {
    esp_pm_lock_delete(sleep_lock);
  }
}

void MAX30102::reset() {
  ESP_LOGI(TAG, "Resetting...");
  for (int i = 0; i < 10; i++) {
    esp_err_t result = write_reg_checked(regs::MODE_CONFIG,
                                         regs::ModeConfig{
                                             .mode = regs::Mode::HEART_RATE,
                                             .reset = true,
                                             .shutdown = false,
                                         }
                                             .raw_value);
    if ((result == ESP_ERR_INVALID_STATE ||
         result == ESP_ERR_INVALID_RESPONSE) &&
        i < 10) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    } else if (result == ESP_OK) {
      break;
    } else {
      ESP_ERROR_CHECK(result);
    }
  }
  while (regs::ModeConfig{.raw_value = read_reg(regs::MODE_CONFIG)}.reset &&
         !regs::InterruptMask1{.raw_value = read_reg(regs::INTERRUPT_STATUS_1)}
              .power_ready) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  ESP_LOGI(TAG, "Reset");

  ESP_ERROR_CHECK(gpio_isr_handler_add(
      pin_config.interrupt, handle_almost_full_interrupt_static, this));
  ESP_ERROR_CHECK(
      gpio_wakeup_enable(pin_config.interrupt, GPIO_INTR_LOW_LEVEL));
  gpio_set_intr_type(pin_config.interrupt, GPIO_INTR_NEGEDGE);
}

void MAX30102::start() {
  reset();
  read_reg(regs::INTERRUPT_STATUS_1);
  write_reg(regs::INTERRUPT_ENABLE_1,
            regs::InterruptMask1{
                .power_ready = false,
                .ambient_light_cancellation_overflow = false,
                .fifo_data_ready = false,
                .fifo_almost_full = true,
            }
                .raw_value);
  write_reg(regs::FIFO_CONFIG,
            regs::FifoConfig{
                .fifo_almost_full_free_limit = 32 - QUEUE_ALMOST_FULL_THRESHOLD,
                .fifo_rolls_on_full = true,
                .sample_averaging = 3, // 4-sample averaging
            }
                .raw_value);
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::HEART_RATE,
                .reset = false,
                .shutdown = true,
            }
                .raw_value);
  write_reg(regs::SPO2_CONFIG,
            regs::SpO2Config{
                .led_pulse_width = 0b11,   // 419.75 µs, 18-bit ADC
                .spo2_sample_rate = 0b010, // 400 samples/s
                .spo2_adc_range = 0b11,    // 62.50 pA LSB, 16384 nA full range
            }
                .raw_value);
  write_reg(regs::LED_PULSE_AMPLITUDE_RED, 0x20); // 6.4 mA
  write_reg(regs::LED_PULSE_AMPLITUDE_IR, 0x20);  // 6.4 mA
  ESP_LOGI(TAG, "Configured");
  ESP_LOGI(TAG, "%02X: %02X", regs::FIFO_CONFIG, read_reg(regs::FIFO_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::MODE_CONFIG, read_reg(regs::MODE_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::SPO2_CONFIG, read_reg(regs::SPO2_CONFIG));
  ESP_LOGI(TAG, "%02X: %02X", regs::LED_PULSE_AMPLITUDE_RED,
           read_reg(regs::LED_PULSE_AMPLITUDE_RED));
  ESP_LOGI(TAG, "%02X: %02X", regs::LED_PULSE_AMPLITUDE_IR,
           read_reg(regs::LED_PULSE_AMPLITUDE_IR));
  is_spo2 = false;
  queue_len_needs_update.store(false, std::memory_order_relaxed);
  queue_len = 0;
}

void MAX30102::set_hr() {
  is_spo2 = false;
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::HEART_RATE,
                .reset = false,
                .shutdown = false,
            }
                .raw_value);
  ESP_LOGI(TAG, "Configured for HR");
}

void MAX30102::set_spo2() {
  is_spo2 = true;
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::SPO2,
                .reset = false,
                .shutdown = false,
            }
                .raw_value);
  ESP_LOGI(TAG, "Configured for HR + SpO2");
}

void MAX30102::set_sleep() {
  write_reg(regs::MODE_CONFIG,
            regs::ModeConfig{
                .mode = regs::Mode::HEART_RATE,
                .reset = false,
                .shutdown = true,
            }
                .raw_value);
  ESP_LOGI(TAG, "Configured for sleep");
}

bool MAX30102::sample_is_ready() {
  if (queue_len_needs_update.load(std::memory_order_relaxed)) {
    update_queue_len();
  }
  return queue_len > 0;
}

void MAX30102::wait_for_samples() {
  for (;;) {
    if (queue_len_needs_update.load(std::memory_order_relaxed)) {
      update_queue_len();
    }
    if (queue_len > 0) {
      // ESP_LOGI(TAG, "Return");
      return;
    } else {
      // ESP_LOGI(TAG, "Waiting %zu %u", queue_len,
      //          queue_len_needs_update.load(std::memory_order_relaxed));
      waiting_task = xTaskGetCurrentTaskHandle();
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      // ESP_LOGI(TAG, "Woken up %zu %u", queue_len,
      //          queue_len_needs_update.load(std::memory_order_relaxed));
    }
  }
}

array<uint32_t, 1> MAX30102::read_hr_sample() {
  return read_sample_channels<1>();
}

array<uint32_t, 2> MAX30102::read_spo2_sample() {
  return read_sample_channels<2>();
}

uint8_t MAX30102::read_reg(const uint8_t reg_addr) {
  // ESP_LOGD(TAG, "Reading from %02X", reg_addr);
  const array<uint8_t, 1> write_data{reg_addr};
  array<uint8_t, 1> read_data{};
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_acquire(sleep_lock));
  }
  ESP_ERROR_CHECK(i2c_master_transmit_receive(
      this->dev, write_data.data(), write_data.size(), read_data.data(),
      read_data.size(), this->transfer_timeout_ms));
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_release(sleep_lock));
  }
  return read_data[0];
}

esp_err_t MAX30102::write_reg_checked(const uint8_t reg_addr,
                                      const uint8_t data) {
  ESP_LOGI(TAG, "Writing %02X to %02X", data, reg_addr);
  const array<uint8_t, 2> write_data{reg_addr, data};
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_acquire(sleep_lock));
  }
  const esp_err_t result =
      i2c_master_transmit(this->dev, write_data.data(), write_data.size(),
                          this->transfer_timeout_ms);
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_release(sleep_lock));
  }
  return result;
}

void MAX30102::write_reg(const uint8_t reg_addr, const uint8_t data) {
  ESP_ERROR_CHECK(write_reg_checked(reg_addr, data));
}

template <uint8_t channels>
array<uint32_t, channels> MAX30102::read_sample_channels() {
  queue_len--;
  const array<uint8_t, 1> write_data{max30102_regs::FIFO_DATA};
  array<uint8_t, 3 * channels> read_data{};
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_acquire(sleep_lock));
  }
  ESP_ERROR_CHECK(i2c_master_transmit_receive(
      this->dev, write_data.data(), write_data.size(), read_data.data(),
      read_data.size(), this->transfer_timeout_ms));
  if (sleep_lock) {
    ESP_ERROR_CHECK(esp_pm_lock_release(sleep_lock));
  }
  array<uint32_t, channels> result;
  for (int i = 0; i < channels; i++) {
    uint8_t *sample_data = &read_data[i * 3];
    result[i] = ((uint32_t)(sample_data[0]) << 16) |
                ((uint32_t)(sample_data[1]) << 8) | (uint32_t)(sample_data[2]);
  }
  return result;
}

void MAX30102::handle_almost_full_interrupt() {
  queue_len_needs_update.store(true, std::memory_order_relaxed);
  gpio_set_intr_type(pin_config.interrupt, GPIO_INTR_NEGEDGE);
  // ESP_DRAM_LOGI(TAG, "Almost full interrupt");
  if (waiting_task) {
    int higher_priority_task_awoken;
    vTaskNotifyGiveFromISR(waiting_task, &higher_priority_task_awoken);
    if (higher_priority_task_awoken) {
      portYIELD_FROM_ISR();
    }
  }
}

void MAX30102::update_queue_len() {
  queue_len_needs_update.store(false, std::memory_order_relaxed);
  read_reg(regs::INTERRUPT_STATUS_1);
  queue_len = (size_t)((read_reg(regs::FIFO_WRITE_POINTER) -
                        read_reg(regs::FIFO_READ_POINTER) + 31) &
                       31) +
              1;
  gpio_set_intr_type(pin_config.interrupt, GPIO_INTR_LOW_LEVEL);
  // ESP_LOGI(TAG, "Updated queue len %zu", queue_len);
}
