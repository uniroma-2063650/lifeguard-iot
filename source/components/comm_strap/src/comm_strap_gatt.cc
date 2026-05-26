#include "comm_common.hh"
#include "comm_strap.hh"
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <services/gatt/ble_svc_gatt.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

constexpr const ble_uuid16_t HEART_RATE_SVC_UUID = BLE_UUID16_INIT(0x180D);
constexpr const ble_uuid16_t HEART_RATE_CHR_UUID = BLE_UUID16_INIT(0x2A37);
constexpr const ble_uuid16_t SPO2_SVC_UUID = BLE_UUID16_INIT(0x1822);
constexpr const ble_uuid16_t SPO2_CHR_UUID = BLE_UUID16_INIT(0x2AF9);
constexpr const ble_uuid16_t DEVICE_INFO_SVC_UUID = BLE_UUID16_INIT(0x180A);
constexpr const ble_uuid16_t ROOM_CHR_UUID = BLE_UUID16_INIT(0x2C34);
constexpr const ble_uuid16_t BED_CHR_UUID = BLE_UUID16_INIT(0x2A9A);

#define ACCESS_CHR_OPT_IMPL(name)                                              \
  int CommStrap::access_##name##_chr(                                          \
      const uint16_t conn_handle, const uint16_t attr_handle,                  \
      struct ble_gatt_access_ctxt *const ctxt) {                               \
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR ||                             \
        attr_handle != name##_chr_val_handle) {                                \
      return BLE_ATT_ERR_REQ_NOT_SUPPORTED;                                    \
    }                                                                          \
    const auto value = patient_data.name.value_or(-1);                         \
    BLE_ERROR_CHECK(os_mbuf_append(ctxt->om, &value, sizeof(value)));          \
    return 0;                                                                  \
  }

#define ACCESS_CHR_IMPL(name)                                                  \
  int CommStrap::access_##name##_chr(                                          \
      const uint16_t conn_handle, const uint16_t attr_handle,                  \
      struct ble_gatt_access_ctxt *const ctxt) {                               \
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR ||                             \
        attr_handle != name##_chr_val_handle) {                                \
      return BLE_ATT_ERR_REQ_NOT_SUPPORTED;                                    \
    }                                                                          \
    BLE_ERROR_CHECK(os_mbuf_append(ctxt->om, &name, sizeof(name)));            \
    return 0;                                                                  \
  }

ACCESS_CHR_OPT_IMPL(heart_rate);
ACCESS_CHR_OPT_IMPL(spo2);
ACCESS_CHR_IMPL(room);
ACCESS_CHR_IMPL(bed);

void CommStrap::reset_gatt_subscriptions() {
  heart_rate_chr_conn_handle.reset();
  spo2_chr_conn_handle.reset();
}

void CommStrap::subscribe_gatt(
    const decltype(ble_gap_event::subscribe) subscribe) {
  std::optional<uint16_t> *conn_handle_out;
  bool* needs_new_out;
  if (subscribe.attr_handle == heart_rate_chr_val_handle) {
    conn_handle_out = &heart_rate_chr_conn_handle;
    needs_new_out = &needs_new_hr;
  } else if (subscribe.attr_handle == spo2_chr_val_handle) {
    conn_handle_out = &spo2_chr_conn_handle;
    needs_new_out = &needs_new_spo2;
  } else {
    return;
  }
  if (subscribe.cur_indicate) {
    conn_handle_out->emplace(subscribe.conn_handle);
    *needs_new_out = true;
  } else {
    conn_handle_out->reset();
  }
}

void CommStrap::signal_gatt_heart_rate_change() {
  if (heart_rate_chr_conn_handle.has_value()) {
    ESP_LOGI(TAG, "Signaling HR update to %" PRIu16,
             heart_rate_chr_conn_handle.value_or(-1));
    ble_gatts_indicate(heart_rate_chr_conn_handle.value(),
                       heart_rate_chr_val_handle);
  }
}

void CommStrap::signal_gatt_spo2_change() {
  if (spo2_chr_conn_handle.has_value()) {
    ESP_LOGI(TAG, "Signaling SPO2 update to %" PRIu16,
             spo2_chr_conn_handle.value_or(-1));
    ble_gatts_indicate(spo2_chr_conn_handle.value(), spo2_chr_val_handle);
  }
}

static ble_gatt_cpfd heart_rate_chr_cpfds[] = {
    {
        .format = BLE_GATT_CHR_FORMAT_UINT8,
        .exponent = 0,
        .unit = BLE_GATT_CHR_UNIT_BEATS_PER_MINUTE,
        .name_space = BLE_GATT_CHR_NAMESPACE_BT_SIG,
        .description = BLE_GATT_CHR_BT_SIG_DESC_MAIN,
    },
    {.format = 0}};
static ble_gatt_cpfd spo2_chr_cpfds[] = {
    {
        .format = BLE_GATT_CHR_FORMAT_UINT8,
        .exponent = 0,
        .unit = BLE_GATT_CHR_UNIT_PERCENTAGE,
        .name_space = BLE_GATT_CHR_NAMESPACE_BT_SIG,
        .description = BLE_GATT_CHR_BT_SIG_DESC_MAIN,
    },
    {.format = 0}};
static ble_gatt_cpfd room_bed_chr_cpfds[] = {
    {
        .format = BLE_GATT_CHR_FORMAT_UINT8,
        .exponent = 0,
        .unit = BLE_GATT_CHR_UNIT_UNITLESS,
        .name_space = BLE_GATT_CHR_NAMESPACE_BT_SIG,
        .description = BLE_GATT_CHR_BT_SIG_DESC_MAIN,
    },
    {.format = 0}};

void CommStrap::init_gatt() {
  heart_rate_chrs[0] = {.uuid = &HEART_RATE_CHR_UUID.u,
                        .access_cb = access_heart_rate_chr_static,
                        .arg = this,
                        .descriptors = nullptr,
                        .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                        .min_key_size = 0,
                        .val_handle = &heart_rate_chr_val_handle,
                        .cpfd = heart_rate_chr_cpfds};
  heart_rate_chrs[1] = {.uuid = nullptr};
  spo2_chrs[0] = {.uuid = &SPO2_CHR_UUID.u,
                  .access_cb = access_spo2_chr_static,
                  .arg = this,
                  .descriptors = nullptr,
                  .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_INDICATE,
                  .min_key_size = 0,
                  .val_handle = &spo2_chr_val_handle,
                  .cpfd = spo2_chr_cpfds};
  spo2_chrs[1] = {.uuid = nullptr};
  device_info_chrs[0] = {.uuid = &ROOM_CHR_UUID.u,
                         .access_cb = access_room_chr_static,
                         .arg = this,
                         .descriptors = nullptr,
                         .flags = BLE_GATT_CHR_F_READ,
                         .min_key_size = 0,
                         .val_handle = &room_chr_val_handle,
                         .cpfd = room_bed_chr_cpfds};
  device_info_chrs[1] = {.uuid = &BED_CHR_UUID.u,
                         .access_cb = access_bed_chr_static,
                         .arg = this,
                         .descriptors = nullptr,
                         .flags = BLE_GATT_CHR_F_READ,
                         .min_key_size = 0,
                         .val_handle = &bed_chr_val_handle,
                         .cpfd = room_bed_chr_cpfds};
  device_info_chrs[2] = {.uuid = nullptr};

  gatt_svr_svcs[0] = {
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = &HEART_RATE_SVC_UUID.u,
      .includes = nullptr,
      .characteristics = heart_rate_chrs,
  };
  gatt_svr_svcs[1] = {
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = &SPO2_SVC_UUID.u,
      .includes = nullptr,
      .characteristics = spo2_chrs,
  };
  gatt_svr_svcs[2] = {
      .type = BLE_GATT_SVC_TYPE_PRIMARY,
      .uuid = &DEVICE_INFO_SVC_UUID.u,
      .includes = nullptr,
      .characteristics = device_info_chrs,
  };
  gatt_svr_svcs[3] = {.type = BLE_GATT_SVC_TYPE_END};

  ble_svc_gatt_init();
  BLE_ERROR_CHECK(ble_gatts_count_cfg(gatt_svr_svcs));
  BLE_ERROR_CHECK(ble_gatts_add_svcs(gatt_svr_svcs));
}

#pragma GCC diagnostic pop
