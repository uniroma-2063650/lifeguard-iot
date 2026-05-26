#pragma once

#include "comm_common.hh"
#include <cassert>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <host/ble_gap.h>
#include <optional>

#define ACCESS_CHR(name)                                                       \
  static int access_##name##_chr_static(                                       \
      const uint16_t conn_handle, const uint16_t attr_handle,                  \
      struct ble_gatt_access_ctxt *const ctxt, void *const arg) {              \
    return ((CommStrap *)arg)                                                  \
        ->access_##name##_chr(conn_handle, attr_handle, ctxt);                 \
  }                                                                            \
  int access_##name##_chr(uint16_t conn_handle, uint16_t attr_handle,          \
                          struct ble_gatt_access_ctxt *ctxt);

struct CommStrap {
  static CommStrap &init(const uint16_t room, const uint8_t bed,
                         const UBaseType_t priority = 0) {
    assert(!is_running);
    instance.start(room, bed, priority);
    is_running = true;
    return instance;
  }

  static CommStrap &get() {
    assert(is_running);
    return instance;
  }

  void set_heart_rate(std::optional<uint8_t> heart_rate);
  void set_spo2(std::optional<uint8_t> spo2);

private:
  static bool is_running;
  static CommStrap instance;

  TaskHandle_t task = nullptr;

  char device_name[21];
  uint16_t room;
  uint8_t bed;
  uint16_t room_chr_val_handle, bed_chr_val_handle;

  CommPatientData patient_data;
  bool needs_new_hr, needs_new_spo2;
  uint16_t heart_rate_chr_val_handle, spo2_chr_val_handle;
  std::optional<uint16_t> heart_rate_chr_conn_handle{}, spo2_chr_conn_handle{};

  ble_gatt_chr_def heart_rate_chrs[2], spo2_chrs[2], device_info_chrs[3];
  ble_gatt_svc_def gatt_svr_svcs[4];

  void start_advertising();

  static int handle_gap_event_static(ble_gap_event *event, void *arg) {
    return ((CommStrap *)arg)->handle_gap_event(event);
  }
  int handle_gap_event(ble_gap_event *event);

  void init_gap();
  void start_gap();

  ACCESS_CHR(heart_rate);
  ACCESS_CHR(spo2);
  ACCESS_CHR(bed);
  ACCESS_CHR(room);

  void reset_gatt_subscriptions();
  void subscribe_gatt(decltype(ble_gap_event::subscribe) subscribe);

  void signal_gatt_heart_rate_change();
  void signal_gatt_spo2_change();

  void init_gatt();

  void init_nvs();
  void init_config();

  static void initialized_static() { get().initialized(); }
  void initialized();

  static void run_static(void *arg) { ((CommStrap *)arg)->run(); }
  void run();

  void start(uint16_t room, uint8_t bed, UBaseType_t priority);

  CommStrap() {}
};
