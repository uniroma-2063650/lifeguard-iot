#include <algorithm>
#include <array>
#include <freertos/FreeRTOS.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <optional>

#include "alert.hh"
#include "comm_hub.hh"
#include "i2c.hh"
#include "monitor.hh"

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

#define TASK(name, ...)                                                        \
  void name(void *args) { __VA_ARGS__ vTaskDelete(nullptr); }

static AlertSystem alert(AlertPinConfig::DISCONNECTED);

static TaskHandle_t redraw_monitor_task_handle;
static TaskHandle_t process_task_handle;

constexpr uint8_t ROOM = 0;

static QueueHandle_t redraw_monitor_queue;

TASK(redraw_monitor, {
  I2CMasterBus bus(I2cMasterBusConfig{
      .sda = GPIO_NUM_17,
      .scl = GPIO_NUM_18,
      .clock_source = I2C_CLK_SRC_DEFAULT,
  });

  Monitor monitor(bus.handle);

  ESP_LOGI("redraw_monitor", "Started");
  std::array<std::optional<PatientData>, 4> patient_data;
  for (;;) {
    monitor.draw_patient_data(patient_data);
    monitor.flush();
    while (xQueueReceive(redraw_monitor_queue, &patient_data, portMAX_DELAY) ==
           pdFALSE)
      ;
    ESP_LOGI("redraw_monitor", "Redrawing patient data");
  }
});

TASK(process, {
  CommHub &comm = CommHub::get();
  std::array<std::optional<PatientData>, 4> patient_data;

  const auto calc_warning = [](const PatientData &patient) {
    if (patient.heart_rate.has_value() &&
        (patient.heart_rate.value() > 120 || patient.heart_rate.value() < 40)) {
      return true;
    }
    if (patient.spo2.has_value() && patient.spo2.value() < 95) {
      return true;
    }
    return false;
  };

  const auto calc_critical = [](const PatientData &patient) {
    if (patient.heart_rate.has_value() &&
        (patient.heart_rate.value() > 160 || patient.heart_rate.value() < 30)) {
      return true;
    }
    if (patient.spo2.has_value() && patient.spo2.value() < 90) {
      return true;
    }
    return false;
  };

  ESP_LOGI("process", "Started");

  AlertState alert_state = AlertState::OK;

  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    const auto maybe_update = comm.receive_update(portMAX_DELAY);
    if (!maybe_update.has_value()) {
      continue;
    }
    const auto &update = maybe_update.value();

    ESP_LOGI("process", "Received heart data update for patient %" PRIu8,
             update.bed);
    if (update.data.has_value()) {
      const CommPatientData &new_data = update.data.value();
      if (!patient_data[update.bed].has_value()) {
        patient_data[update.bed].emplace(PatientData{
            .heart_rate = {}, .spo2 = {}, .state = PatientState::OK});
      }
      PatientData &patient = patient_data[update.bed].value();
      patient.heart_rate = new_data.heart_rate;
      patient.spo2 = new_data.spo2;
      patient.state = calc_critical(patient)  ? PatientState::CRITICAL
                      : calc_warning(patient) ? PatientState::WARNING
                                              : PatientState::OK;
    } else {
      patient_data[update.bed].reset();
    }

    ESP_LOGI("process", "Sending redraw request");
    xQueueOverwrite(redraw_monitor_queue, &patient_data);

    const bool new_has_warning = std::any_of(
        patient_data.begin(), patient_data.end(), [](const auto &patient) {
          return patient.has_value() &&
                 patient.value().state == PatientState::WARNING;
        });
    const bool new_has_critical = std::any_of(
        patient_data.begin(), patient_data.end(), [](const auto &patient) {
          return patient.has_value() &&
                 patient.value().state == PatientState::CRITICAL;
        });
    const AlertState new_alert_state = new_has_critical  ? AlertState::CRITICAL
                                       : new_has_warning ? AlertState::WARNING
                                                         : AlertState::OK;
    if (new_alert_state != alert_state) {
      alert_state = new_alert_state;
      ESP_LOGI("process", "%s",
               new_alert_state != AlertState::OK ? "New warnings generated"
                                                 : "All warnings resolved");
      alert.set_state(alert_state);
    }
  }
});

extern "C" void app_main(void) {
  CommHub::init(ROOM);

  alert.set_pin_config(AlertPinConfig::DEFAULT);
  alert.start();

  redraw_monitor_queue =
      xQueueCreate(1, sizeof(std::array<std::optional<PatientData>, 4>));
  PD_ERROR_CHECK(xTaskCreate(redraw_monitor, "redraw_monitor", 8192, nullptr, 5,
                             &redraw_monitor_task_handle));

  PD_ERROR_CHECK(
      xTaskCreate(process, "process", 4096, nullptr, 5, &process_task_handle));
}
