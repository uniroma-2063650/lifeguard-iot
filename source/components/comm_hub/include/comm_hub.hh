#pragma once

#include "comm_common.hh"
#include <cassert>
#include <esp_nimble_cfg.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <unordered_map>

#include <esp_central.h>

template <> struct std::equal_to<ble_addr_t> {
  inline bool operator()(const ble_addr_t &l, const ble_addr_t &r) const {
    return !memcmp(&l, &r, sizeof(ble_addr_t));
  }
};

template <> struct std::hash<ble_addr_t> {
  inline std::size_t operator()(const ble_addr_t &k) const {
    union {
      size_t result;
      uint8_t bytes[sizeof(size_t)];
    } result{.result = 0};
    size_t out_i = 0;
    {
      result.bytes[out_i] ^= k.type;
      out_i++;
      out_i &= (sizeof(size_t) - 1);
    }
    for (size_t val_i = 0; val_i < sizeof(k.val); val_i++) {
      result.bytes[out_i] ^= k.val[val_i];
      out_i++;
      out_i &= (sizeof(size_t) - 1);
    }
    return result.result;
  }
};

struct CommPatientUpdateData {
  uint8_t bed;
  std::optional<CommPatientData> data;
};

struct CommHub {
  static CommHub &init(const uint16_t room, const UBaseType_t priority = 0) {
    assert(!is_running);
    instance.start(room, priority);
    is_running = true;
    return instance;
  }

  static CommHub &get() {
    assert(is_running);
    return instance;
  }

  inline QueueHandle_t getQueue() {
    return queue;
  }

  std::optional<CommPatientUpdateData> receive_update(TickType_t timeout);

private:
  static constexpr const TickType_t QUEUE_SEND_TIMEOUT = pdMS_TO_TICKS(100);

  static bool is_running;
  static CommHub instance;

  TaskHandle_t task = nullptr;
  QueueHandle_t queue = nullptr;

  char device_name[20];
  uint16_t room;

  std::unordered_map<ble_addr_t, uint8_t> addr_to_bed{};
  std::unordered_map<uint16_t, uint8_t> conn_to_bed{};
  std::unordered_map<uint16_t, uint16_t> conn_to_heart_rate_val_handle{};
  std::unordered_map<uint16_t, uint16_t> conn_to_spo2_val_handle{};
  std::unordered_map<uint8_t, CommPatientData> patient_data{};

  void start_scanning();
  void connect_if_matches(ble_hs_adv_fields fields, ble_gap_disc_desc disc);

  static int handle_gap_event_static(ble_gap_event *const event,
                                     void *const arg) {
    return ((CommHub *)arg)->handle_gap_event(event);
  }
  int handle_gap_event(ble_gap_event *event);

  void init_gap();
  void start_gap();
  void disconnect(uint16_t conn_handle);
  void disconnected(const ble_gap_conn_desc &conn_desc);
  void connected(uint16_t conn_handle, ble_addr_t addr);

  void init_nvs();
  void init_config();

  static void initialized_static() { get().initialized(); }
  void initialized();

  static void run_static(void *arg) { ((CommHub *)arg)->run(); }
  void run();

  bool subscribe_to_gatt_chr(const peer *peer, const ble_uuid_t *svc_uuid,
                             const ble_uuid_t *chr_uuid);
  static void gatt_discovery_done_static(const struct peer *const peer,
                                         const int status, void *const arg) {
    ((CommHub *)arg)->gatt_discovery_done(peer, status);
  }
  void gatt_discovery_done(const struct peer *peer, int status);
  bool init_gatt(uint16_t conn_handle);
  void gatt_chr_updated(uint16_t conn_handle, uint16_t attr_handle,
                        os_mbuf *data);

  void start(uint16_t room, UBaseType_t priority);

  CommHub() {}
};
