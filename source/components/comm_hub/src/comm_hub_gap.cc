#include "comm_common.hh"
#include "comm_hub.hh"
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <nvs_flash.h>
#include <services/gap/ble_svc_gap.h>

constexpr const uint32_t CONNECT_TIMEOUT_MS = 30000;

void CommHub::disconnect(const uint16_t conn_handle) {
  ble_gap_terminate(conn_handle, BLE_ERR_REM_USER_CONN_TERM);
  ble_gap_conn_desc conn_desc;
  BLE_ERROR_CHECK_RETURN(ble_gap_conn_find(conn_handle, &conn_desc), );
  disconnected(conn_desc);
}

void CommHub::disconnected(const ble_gap_conn_desc &conn_desc) {
  if (conn_to_bed.contains(conn_desc.conn_handle)) {
    const uint8_t bed = conn_to_bed[conn_desc.conn_handle];
    if (patient_data.contains(bed)) {
      CommPatientUpdateData update_data{
          .bed = bed,
          .data{},
      };
      xQueueSend(queue, &update_data, QUEUE_SEND_TIMEOUT);
      patient_data.erase(bed);
    }
    conn_to_bed.erase(conn_desc.conn_handle);
  }
  conn_to_heart_rate_val_handle.erase(conn_desc.conn_handle);
  conn_to_spo2_val_handle.erase(conn_desc.conn_handle);
}

void CommHub::connected(const uint16_t conn_handle, const ble_addr_t addr) {
  assert(addr_to_bed.contains(addr));
  conn_to_bed.emplace(conn_handle, addr_to_bed[addr]);
  addr_to_bed.erase(addr);
}

void CommHub::start_scanning() {
  BLE_ERROR_CHECK(ble_hs_util_ensure_addr(0));
  uint8_t own_addr_type;
  uint8_t own_addr[6];
  BLE_ERROR_CHECK(ble_hs_id_infer_auto(0, &own_addr_type));
  BLE_ERROR_CHECK(ble_hs_id_copy_addr(own_addr_type, own_addr, nullptr));
  ESP_LOGI(TAG, "Initializing BLE with address: " PRINT_ADDR,
           PRINT_ADDR_VALS(own_addr));

  ble_gap_disc_params disc_params{};

  disc_params.passive = true;
  disc_params.filter_duplicates = true;

  BLE_ERROR_CHECK(ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params,
                               handle_gap_event_static, this));
}

void CommHub::connect_if_matches(const ble_hs_adv_fields fields,
                                 const ble_gap_disc_desc disc) {
  if (disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_ADV_IND &&
      disc.event_type != BLE_HCI_ADV_RPT_EVTYPE_DIR_IND)
    return;
  if (fields.mfg_data_len != 7 || memcmp(fields.mfg_data, "LFGD", 4))
    return;

  const uint16_t room =
      (((uint16_t)fields.mfg_data[4]) << 8) | fields.mfg_data[5];
  if (room != this->room)
    return;
  const uint8_t bed = fields.mfg_data[6];

  ESP_LOGI(TAG, "Found matching device: room %u, bed %u, addr " PRINT_ADDR,
           room, bed, PRINT_ADDR_VALS(disc.addr.val));

  BLE_ERROR_CHECK(ble_gap_disc_cancel());

  uint8_t own_addr_type;
  BLE_ERROR_CHECK(ble_hs_id_infer_auto(0, &own_addr_type));

  BLE_ERROR_CHECK_RETURN(ble_gap_connect(own_addr_type, &disc.addr,
                                         CONNECT_TIMEOUT_MS, nullptr,
                                         handle_gap_event_static, this), );

  addr_to_bed.emplace(disc.addr, bed);
}

int CommHub::handle_gap_event(ble_gap_event *event) {
  switch (event->type) {
  case BLE_GAP_EVENT_DISC: {
    ble_hs_adv_fields adv_fields;
    BLE_ERROR_CHECK(ble_hs_adv_parse_fields(&adv_fields, event->disc.data,
                                            event->disc.length_data));
    print_adv_fields(&adv_fields);
    connect_if_matches(adv_fields, event->disc);
    break;
  }

  case BLE_GAP_EVENT_CONNECT: {
    if (event->connect.status == 0) {
      ESP_LOGI(TAG, "Connection established");

      ble_gap_conn_desc conn_desc;
      BLE_ERROR_CHECK(
          ble_gap_conn_find(event->connect.conn_handle, &conn_desc));
      print_conn_desc(&conn_desc);

      BLE_ERROR_CHECK_RETURN(peer_add(event->connect.conn_handle), 0);

      connected(event->connect.conn_handle, conn_desc.peer_id_addr);

      // BLE_ERROR_CHECK_RETURN(
      //     ble_gap_security_initiate(event->connect.conn_handle),
      //     ble_gap_terminate(event->connect.conn_handle,
      //                       BLE_ERR_REM_USER_CONN_TERM));
      // ESP_LOGI(TAG, "Connection secured");

      init_gatt(conn_desc.conn_handle);
    } else {
      ESP_LOGE(TAG, "Connection failed; status: %d", event->connect.status);
      start_scanning();
    }
    break;
  }

  case BLE_GAP_EVENT_DISCONNECT: {
    ESP_LOGI(TAG, "Disconnect; reason: %d", event->disconnect.reason);
    print_conn_desc(&event->disconnect.conn);
    peer_delete(event->disconnect.conn.conn_handle);
    start_scanning();
    disconnected(event->disconnect.conn);
    break;
  }

  case BLE_GAP_EVENT_ENC_CHANGE: {
    ESP_LOGI(TAG, "Encryption change event; status: %d",
             event->enc_change.status);
    ble_gap_conn_desc conn_desc;
    BLE_ERROR_CHECK(
        ble_gap_conn_find(event->enc_change.conn_handle, &conn_desc));
    print_conn_desc(&conn_desc);
    break;
  }

  case BLE_GAP_EVENT_NOTIFY_RX: {
    ESP_LOGI(TAG,
             "Received %s; conn_handle: %" PRIu16 " attr_handle: %" PRIu16 " "
             "attr_len: %zu",
             event->notify_rx.indication ? "indication" : "notification",
             event->notify_rx.conn_handle, event->notify_rx.attr_handle,
             OS_MBUF_PKTLEN(event->notify_rx.om));
    gatt_chr_updated(event->notify_rx.conn_handle, event->notify_rx.attr_handle,
                     event->notify_rx.om);
    break;
  }

  case BLE_GAP_EVENT_MTU: {
    ESP_LOGI(TAG,
             "MTU updated; conn_handle: %" PRIu16 ", cid: %" PRIu16
             ", mtu: %" PRIu16,
             event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
    break;
  }

  default: {
    ESP_LOGD(TAG, "Unhandled event %" PRIu8, event->type);
  }
  }
  return 0;
}

void CommHub::init_gap() {
  ble_svc_gap_init();
  BLE_ERROR_CHECK(ble_svc_gap_device_name_set(device_name));
}

void CommHub::start_gap() { start_scanning(); }
