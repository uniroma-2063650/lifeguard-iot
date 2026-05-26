#include "comm_common.hh"
#include "comm_hub.hh"
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>
#include <services/gap/ble_svc_gap.h>

constexpr const ble_uuid16_t HEART_RATE_SVC_UUID = BLE_UUID16_INIT(0x180D);
constexpr const ble_uuid16_t HEART_RATE_CHR_UUID = BLE_UUID16_INIT(0x2A37);
constexpr const ble_uuid16_t SPO2_SVC_UUID = BLE_UUID16_INIT(0x1822);
constexpr const ble_uuid16_t SPO2_CHR_UUID = BLE_UUID16_INIT(0x2AF9);

int subscribe_done(uint16_t conn_handle, const struct ble_gatt_error *error,
                   struct ble_gatt_attr *attr, void *arg) {
  if (error->status == 0) {
    ESP_LOGI(TAG, "Subscribed successfully");
  } else {
    ESP_LOGE(TAG, "Error subscribing to " PRIu16 ", code " PRIu16,
             error->att_handle, error->status);
  }
  return 0;
}

bool CommHub::subscribe_to_gatt_chr(const peer *const peer,
                                    const ble_uuid_t *const svc_uuid,
                                    const ble_uuid_t *const chr_uuid) {
  constexpr const ble_uuid16_t CCCD_DESC_UUID =
      BLE_UUID16_INIT(BLE_GATT_DSC_CLT_CFG_UUID16);

  const peer_dsc *cccd = peer_dsc_find_uuid(
      peer, svc_uuid, chr_uuid, (const ble_uuid_t *)&CCCD_DESC_UUID);
  if (!cccd) {
    ESP_LOGE(TAG, "BLE: Couldn't find CCCD descriptor");
    return false;
  }

  const uint8_t value[2] = {0x02, 0x00};
  BLE_ERROR_CHECK_RETURN(ble_gattc_write_flat(peer->conn_handle,
                                              cccd->dsc.handle, value, 2,
                                              subscribe_done, this),
                         false);
  return true;
}

void CommHub::gatt_discovery_done(const struct peer *const peer,
                                  const int status) {
  if (status != 0) {
    ESP_LOGE(TAG, "BLE discovery failed for peer " PRINT_ADDR,
             PRINT_ADDR_VALS(peer->peer_addr));
    disconnect(peer->conn_handle);
    return;
  }

  const peer_chr *heart_rate_chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&HEART_RATE_SVC_UUID,
                         (const ble_uuid_t *)&HEART_RATE_CHR_UUID);
  if (!heart_rate_chr)
    return;

  const peer_chr *spo2_chr =
      peer_chr_find_uuid(peer, (const ble_uuid_t *)&SPO2_SVC_UUID,
                         (const ble_uuid_t *)&SPO2_CHR_UUID);
  if (!spo2_chr)
    return;

  conn_to_heart_rate_val_handle.emplace(peer->conn_handle,
                                        heart_rate_chr->chr.val_handle);
  conn_to_spo2_val_handle.emplace(peer->conn_handle, spo2_chr->chr.val_handle);

  if (!subscribe_to_gatt_chr(peer, (const ble_uuid_t *)&HEART_RATE_SVC_UUID,
                             (const ble_uuid_t *)&HEART_RATE_CHR_UUID) ||
      !subscribe_to_gatt_chr(peer, (const ble_uuid_t *)&SPO2_SVC_UUID,
                             (const ble_uuid_t *)&SPO2_CHR_UUID)) {
    disconnect(peer->conn_handle);
    return;
  }
}

bool CommHub::init_gatt(const uint16_t conn_handle) {
  BLE_ERROR_CHECK_RETURN(
      peer_disc_all(conn_handle, gatt_discovery_done_static, this),
      (disconnect(conn_handle), false));
  return true;
}

void CommHub::gatt_chr_updated(const uint16_t conn_handle,
                               const uint16_t attr_handle,
                               os_mbuf *const data) {
  if (!conn_to_bed.contains(conn_handle))
    return;

  const uint8_t bed = conn_to_bed[conn_handle];
  const uint16_t heart_rate_val_handle =
      conn_to_heart_rate_val_handle[conn_handle];
  const uint16_t spo2_val_handle = conn_to_spo2_val_handle[conn_handle];

  if (attr_handle != heart_rate_val_handle && attr_handle != spo2_val_handle)
    return;

  if (!patient_data.contains(bed)) {
    patient_data.emplace(bed, CommPatientData{});
  }
  CommPatientData &patient_data = this->patient_data[bed];

#define UPDATE_FIELD(name, desc)                                               \
  decltype(patient_data.heart_rate)::value_type raw_value;                     \
  os_mbuf_copydata(data, 0, sizeof(raw_value), &raw_value);                    \
  const decltype(patient_data.name) value =                                    \
      raw_value == (decltype(patient_data.name)::value_type) - 1               \
          ? decltype(patient_data.name){}                                      \
          : raw_value;                                                         \
  if (value == patient_data.name)                                              \
    return;                                                                    \
  patient_data.name = value;                                                   \
  ESP_LOGI(TAG, "Bed %" PRIu8 " %s updated: %d", bed, desc, raw_value);

  if (attr_handle == heart_rate_val_handle) {
    UPDATE_FIELD(heart_rate, "heart rate");
  } else if (attr_handle == spo2_val_handle) {
    UPDATE_FIELD(spo2, "SpO2");
  }

  CommPatientUpdateData update_data{
      .bed = bed,
      .data = patient_data,
  };
  xQueueSend(queue, &update_data, QUEUE_SEND_TIMEOUT);
}
