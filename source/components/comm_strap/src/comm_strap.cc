#include "comm_strap.hh"
#include "comm_common.hh"
#include <esp_bt.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>

constexpr const char *DEVICE_NAME_PREFIX = "Lfgd Strap ";

extern "C" void ble_store_config_init(void);

bool CommStrap::is_running = false;
CommStrap CommStrap::instance{};

void CommStrap::init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void CommStrap::init_config() {
  ble_hs_cfg.sync_cb = initialized_static;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();
}

void CommStrap::initialized() {
  ESP_ERROR_CHECK(
      esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N18));
  ESP_ERROR_CHECK(esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N18));
  start_gap();
}

void CommStrap::run() {
  nimble_port_run();
  nimble_port_deinit();
  vTaskDelete(nullptr);
}

void CommStrap::set_heart_rate(const std::optional<uint8_t> heart_rate) {
  if (heart_rate == patient_data.heart_rate && !needs_new_hr) {
    return;
  }
  patient_data.heart_rate = heart_rate;
  signal_gatt_heart_rate_change();
  needs_new_hr = false;
}

void CommStrap::set_spo2(const std::optional<uint8_t> spo2) {
  if (spo2 == patient_data.spo2 && !needs_new_spo2) {
    return;
  }
  patient_data.spo2 = spo2;
  signal_gatt_spo2_change();
  needs_new_spo2 = false;
}

void CommStrap::start(const uint16_t room, const uint8_t bed,
                      const UBaseType_t priority) {
  this->room = room;
  this->bed = bed;
  needs_new_hr = false;
  needs_new_spo2 = false;
  snprintf(device_name, sizeof(device_name), "%s%" PRIu16 ":%" PRIu8,
           DEVICE_NAME_PREFIX, room, bed);
  init_nvs();
  ESP_ERROR_CHECK(nimble_port_init());
  init_gap();
  init_gatt();
  init_config();
  PD_ERROR_CHECK(xTaskCreatePinnedToCore(run_static, "comm",
                                         NIMBLE_HS_STACK_SIZE, this, priority,
                                         &task, NIMBLE_CORE));
}
