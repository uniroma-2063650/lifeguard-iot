#include "comm_hub.hh"
#include "comm_common.hh"
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>

constexpr const char *DEVICE_NAME_PREFIX = "Lfgd Hub ";

constexpr const size_t QUEUE_SIZE = 4;

extern "C" void ble_store_config_init(void);

bool CommHub::is_running = false;
CommHub CommHub::instance{};

void CommHub::init_nvs() {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
}

void CommHub::init_config() {
  ble_hs_cfg.sync_cb = initialized_static;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
  ble_store_config_init();
}

void CommHub::initialized() { start_gap(); }

void CommHub::run() {
  nimble_port_run();
  nimble_port_deinit();
  vTaskDelete(nullptr);
}

void CommHub::start(const uint16_t room, const UBaseType_t priority) {
  conn_to_bed.clear();
  addr_to_bed.clear();
  patient_data.clear();

  this->room = room;
  snprintf(device_name, sizeof(device_name), "%s%" PRIu16, DEVICE_NAME_PREFIX,
           room);
  init_nvs();
  ESP_ERROR_CHECK(nimble_port_init());
  init_gap();

#if MYNEWT_VAL(BLE_INCL_SVC_DISCOVERY) ||                                      \
    MYNEWT_VAL(BLE_GATT_CACHING_INCLUDE_SERVICES)
  peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64, 64);
#else
  peer_init(MYNEWT_VAL(BLE_MAX_CONNECTIONS), 64, 64, 64);
#endif

  init_config();
  queue = xQueueCreate(QUEUE_SIZE, sizeof(CommPatientUpdateData));
  assert(queue);
  PD_ERROR_CHECK(xTaskCreatePinnedToCore(run_static, "comm",
                                         NIMBLE_HS_STACK_SIZE, this, priority,
                                         &task, NIMBLE_CORE));
}

std::optional<CommPatientUpdateData>
CommHub::receive_update(TickType_t timeout) {
  CommPatientUpdateData update_data;
  return xQueueReceive(queue, &update_data, timeout) == pdFALSE
             ? std::optional<CommPatientUpdateData>{}
             : update_data;
}
