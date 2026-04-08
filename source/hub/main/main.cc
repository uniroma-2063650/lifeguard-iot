#include <algorithm>
#include <array>
#include <cstdlib>
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
  void name(void *args) { __VA_ARGS__ vTaskDelete(NULL); }

static AlertSystem alert(AlertPinConfig::DISCONNECTED);

static TaskHandle_t produce_test_data_task_handle;
static TaskHandle_t redraw_monitor_task_handle;
static TaskHandle_t process_task_handle;

constexpr uint8_t ROOM = 0;

TASK(produce_test_data, {
  srand(0);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(500));
    const uint8_t room = ROOM;
    const uint8_t bed = rand() % 4;
    CommPacket packet{
        .room = room,
        .bed = bed,
        .kind = CommPacketKind::HEART_DATA,
        .heart_data =
            CommPacketHeartData{
                .hr = rand() % 181,
                .spo2 = 80 + rand() % 21,
            },
    };
    xQueueSend(CommHub::get().getQueue(), &packet, pdMS_TO_TICKS(100));
  }
});

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
        (patient.heart_rate.value() > 140 || patient.heart_rate.value() < 40)) {
      return true;
    }
    if (patient.spo2.has_value() && patient.spo2.value() < 90) {
      return true;
    }
    return false;
  };

  ESP_LOGI("process", "Started");

  bool has_warning = false;

  for (;;) {
    const auto maybe_packet = comm.receivePacket(portMAX_DELAY);
    if (!maybe_packet.has_value()) {
      continue;
    }
    const auto &packet = maybe_packet.value();

    bool updated_data = false;

    switch (packet.kind) {
    case CommPacketKind::HEART_DATA: {
      ESP_LOGI("process", "Received heart data update for patient %u",
               packet.bed);
      if (!patient_data[packet.bed].has_value()) {
        patient_data[packet.bed].emplace(
            PatientData{.heart_rate = {}, .spo2 = {}, .is_warning = false});
      }
      PatientData &patient = patient_data[packet.bed].value();
      patient.heart_rate = packet.heart_data.hr;
      patient.spo2 = packet.heart_data.spo2;
      patient.is_warning = calc_warning(patient);
      updated_data = true;
      break;
    }
    }

    if (updated_data) {
      ESP_LOGI("process", "Sending redraw request");
      xQueueOverwrite(redraw_monitor_queue, &patient_data);

      const bool new_has_warning = std::any_of(
          patient_data.begin(), patient_data.end(), [](const auto &patient) {
            return patient.has_value() && patient.value().is_warning;
          });
      if (new_has_warning != has_warning) {
        has_warning = new_has_warning;
        ESP_LOGI("process", "%s",
                 has_warning ? "New warnings generated"
                             : "All warnings resolved");
        alert.set_state(has_warning ? AlertState::WARNING : AlertState::OK);
      }
    }
  }
});

extern "C" void app_main(void) {
  CommHub::init(true, 0, ROOM);

  alert.set_pin_config(AlertPinConfig::DEFAULT);
  alert.start();

  // TODO: Just for testing
  PD_ERROR_CHECK(xTaskCreate(produce_test_data, "produce_test_data", 4096,
                             nullptr, 5, &produce_test_data_task_handle));

  redraw_monitor_queue =
      xQueueCreate(1, sizeof(std::array<std::optional<PatientData>, 4>));
  PD_ERROR_CHECK(xTaskCreate(redraw_monitor, "redraw_monitor", 8192, nullptr, 5,
                             &redraw_monitor_task_handle));

  PD_ERROR_CHECK(
      xTaskCreate(process, "process", 4096, nullptr, 5, &process_task_handle));
}
