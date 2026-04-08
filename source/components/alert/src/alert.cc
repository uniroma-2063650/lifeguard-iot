#include "alert.hh"
#include <cstdint>
#include <driver/gpio.h>
#include <esp_err.h>
#include <esp_log.h>

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

constexpr const char *TAG = "Alert";

AlertSystem::AlertSystem(const AlertPinConfig pin_config)
    : queue(nullptr), task(nullptr), pin_config(pin_config) {
  apply_pin_config(pin_config);
}

AlertSystem::~AlertSystem() {
  if (queue) {
    vQueueDelete(queue);
  }
  if (task) {
    vTaskDelete(task);
  }
}

void AlertSystem::apply_state(const AlertState state) {
  ESP_LOGD(TAG, "Applying state: %u", (uint8_t)state);
  switch (state) {
  case AlertState::OK: {
    if (pin_config.led_red != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_red, 0));
    if (pin_config.led_green != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_green, 0));
    if (pin_config.buzzer != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.buzzer, 0));
    break;
  }
  case AlertState::WARNING: {
    if (pin_config.led_red != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_red, 1));
    if (pin_config.led_green != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_green, 1));
    if (pin_config.buzzer != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.buzzer, 0));
    break;
  }
  case AlertState::ERROR: {
    if (pin_config.led_red != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_red, 1));
    if (pin_config.led_green != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.led_green, 0));
    if (pin_config.buzzer != GPIO_NUM_NC)
      ESP_ERROR_CHECK(gpio_set_level(pin_config.buzzer, 1));
    break;
  }
  }
}

void AlertSystem::remove_pin_config(const AlertPinConfig config) {
  if (config.led_red != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.led_red));
  if (config.led_green != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.led_green));
  if (config.buzzer != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.buzzer));
}

void AlertSystem::apply_pin_config(const AlertPinConfig config) {
  if (config.led_red != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.led_red));
  if (config.led_green != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.led_green));
  if (config.buzzer != GPIO_NUM_NC)
    ESP_ERROR_CHECK(gpio_reset_pin(config.buzzer));
  const uint64_t pin_bit_mask =
      (config.led_red == GPIO_NUM_NC ? 0 : (uint64_t)1 << config.led_red) |
      (config.led_green == GPIO_NUM_NC ? 0 : (uint64_t)1 << config.led_green) |
      (config.buzzer == GPIO_NUM_NC ? 0 : (uint64_t)1 << config.buzzer);
  if (pin_bit_mask) {
    const gpio_config_t gpio_config_value = {
        .pin_bit_mask = pin_bit_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_config_value));
  }
}

void AlertSystem::set_pin_config(const AlertPinConfig pin_config) {
  remove_pin_config(this->pin_config);
  this->pin_config = pin_config;
  apply_pin_config(pin_config);
}

void AlertSystem::process() {
  AlertState current_state = AlertState::OK;
  apply_state(current_state);
  for (;;) {
    AlertState state;
    if (xQueueReceive(queue, &state, portMAX_DELAY) == pdFALSE)
      continue;
    ESP_LOGI(TAG, "Received state: %u", (uint8_t)state);

    if (state == current_state) {
      continue;
    }
    current_state = state;
    apply_state(state);
  }
  vQueueDelete(queue);
  queue = nullptr;
  vTaskDelete(nullptr);
  task = nullptr;
}

void AlertSystem::process_static(void *const args) {
  AlertSystem *const alert_system = (AlertSystem *)args;
  alert_system->process();
}

TaskHandle_t AlertSystem::start() {
  if (task) {
    ESP_LOGE(TAG, "Task start requested but already running");
  }
  queue = xQueueCreate(1, sizeof(AlertState));
  PD_ERROR_CHECK(xTaskCreate(AlertSystem::process_static, "process_alerts",
                             4096, this, 5, &task));
  return task;
}

void AlertSystem::stop() {
  set_state(AlertState::OK);
  if (queue) {
    vQueueDelete(queue);
    queue = nullptr;
  }
  if (task) {
    vTaskDelete(task);
    task = nullptr;
  }
  apply_state(AlertState::OK);
}

void AlertSystem::set_state(const AlertState state) {
  if (!queue) {
    ESP_LOGE(TAG, "State update requested but processing task not running: %u",
             (uint8_t)state);
  }
  xQueueOverwrite(queue, &state);
}
