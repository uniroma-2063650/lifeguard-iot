#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <soc/gpio_num.h>

enum class AlertState {
  OK,
  WARNING,
  CRITICAL,
};

struct AlertPinConfig {
  gpio_num_t led_red, led_green, buzzer;

  static AlertPinConfig DEFAULT;
  static AlertPinConfig DISCONNECTED;
};

inline AlertPinConfig AlertPinConfig::DEFAULT = {
    .led_red = GPIO_NUM_5,
    .led_green = GPIO_NUM_6,
    .buzzer = GPIO_NUM_7,
};

inline AlertPinConfig AlertPinConfig::DISCONNECTED = {
    .led_red = GPIO_NUM_NC,
    .led_green = GPIO_NUM_NC,
    .buzzer = GPIO_NUM_NC,
};

struct AlertSystem {
private:
  QueueHandle_t queue;
  TaskHandle_t task;

  AlertPinConfig pin_config;

  void apply_state(AlertState state);
  void remove_pin_config(AlertPinConfig config);
  void apply_pin_config(AlertPinConfig config);
  void process();
  static void process_static(void *args);

public:
  AlertSystem(AlertPinConfig pin_config = AlertPinConfig::DEFAULT);
  ~AlertSystem();

  void set_pin_config(AlertPinConfig config);

  TaskHandle_t start();
  void stop();
  void set_state(AlertState state);
};
