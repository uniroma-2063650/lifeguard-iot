#include "comm_common.hh"
#include "comm_strap.hh"
#include <algorithm>
#include <esp_bt.h>
#include <host/ble_gap.h>
#include <host/util/util.h>
#include <services/gap/ble_svc_gap.h>

// Heart Rate Sensor, see Bluetooth spec:
// https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf
constexpr const uint16_t DEVICE_APPEARANCE = 0x0340;

int8_t power_level_idx_to_dbm(const esp_power_level_t level) {
  switch (level) {
  case ESP_PWR_LVL_N24:
    return -24;
  case ESP_PWR_LVL_N21:
    return -21;
  case ESP_PWR_LVL_N18:
    return -18;
  case ESP_PWR_LVL_N15:
    return -15;
  case ESP_PWR_LVL_N12:
    return -12;
  case ESP_PWR_LVL_N9:
    return -9;
  case ESP_PWR_LVL_N6:
    return -6;
  case ESP_PWR_LVL_N3:
    return -3;
  case ESP_PWR_LVL_N0:
    return 0;
  case ESP_PWR_LVL_P3:
    return +3;
  case ESP_PWR_LVL_P6:
    return +6;
  case ESP_PWR_LVL_P9:
    return +9;
  case ESP_PWR_LVL_P12:
    return +12;
  case ESP_PWR_LVL_P15:
    return +15;
  case ESP_PWR_LVL_P18:
    return +18;
  case ESP_PWR_LVL_P20:
    return +20;
  default:
    return 0;
  }
}

static void print_conn_desc(const ble_gap_conn_desc *desc) {
  ESP_LOGI(TAG, "Connection handle: %d", desc->conn_handle);
  ESP_LOGI(TAG, "Own ID address: type: %d, value: " PRINT_ADDR,
           desc->our_id_addr.type, PRINT_ADDR_VALS(desc->our_id_addr.val));
  ESP_LOGI(TAG, "Peer ID address: type: %d, value: " PRINT_ADDR,
           desc->peer_id_addr.type, PRINT_ADDR_VALS(desc->peer_id_addr.val));
  ESP_LOGI(TAG,
           "conn_itvl: %d, conn_latency: %d, supervision_timeout: %d, "
           "encrypted: %d, authenticated: %d, bonded: %d",
           desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
           desc->sec_state.encrypted, desc->sec_state.authenticated,
           desc->sec_state.bonded);
}

void CommStrap::start_advertising() {
  BLE_ERROR_CHECK(ble_hs_util_ensure_addr(0));
  uint8_t addr_type;
  uint8_t addr[6];
  BLE_ERROR_CHECK(ble_hs_id_infer_auto(0, &addr_type));
  BLE_ERROR_CHECK(ble_hs_id_copy_addr(addr_type, addr, nullptr));
  ESP_LOGI(TAG, "Initializing BLE with address: " PRINT_ADDR,
           PRINT_ADDR_VALS(addr));

  ble_hs_adv_fields adv_fields{};

  adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

  adv_fields.tx_pwr_lvl =
      power_level_idx_to_dbm(esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV));
  ESP_LOGI(TAG, "TX power level: %" PRIi8, adv_fields.tx_pwr_lvl);
  adv_fields.tx_pwr_lvl_is_present = true;

  adv_fields.appearance = DEVICE_APPEARANCE;
  adv_fields.appearance_is_present = true;

  adv_fields.le_role = 0x00; // Peripheral
  adv_fields.le_role_is_present = true;

  adv_fields.device_addr = addr;
  adv_fields.device_addr_type = addr_type;
  adv_fields.device_addr_is_present = true;

  const uint8_t mfg_data[7] = {
      'L', 'F', 'G', 'D', (uint8_t)(room >> 8), (uint8_t)room, bed,
  };
  adv_fields.mfg_data = mfg_data;
  adv_fields.mfg_data_len = sizeof(mfg_data);

  BLE_ERROR_CHECK(ble_gap_adv_set_fields(&adv_fields));

  ble_hs_adv_fields rsp_fields{};

  rsp_fields.name = (uint8_t *)device_name;
  rsp_fields.name_len = (uint8_t)strlen(device_name);
  rsp_fields.name_is_complete = true;

  BLE_ERROR_CHECK(ble_gap_adv_rsp_set_fields(&rsp_fields));

  ble_gap_adv_params adv_params{};

  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(1000);
  adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(1010);

  BLE_ERROR_CHECK(ble_gap_adv_start(addr_type, nullptr, BLE_HS_FOREVER,
                                    &adv_params, handle_gap_event_static,
                                    this));
}

int CommStrap::handle_gap_event(ble_gap_event *event) {
  switch (event->type) {
  case BLE_GAP_EVENT_CONNECT: {
    ESP_LOGI(TAG, "Connection %s; status: %d",
             event->connect.status == 0 ? "established" : "failed",
             event->connect.status);
    if (event->connect.status == 0) {
      // Connection succeeded
      ble_gap_conn_desc conn_desc;
      BLE_ERROR_CHECK(
          ble_gap_conn_find(event->connect.conn_handle, &conn_desc));
      print_conn_desc(&conn_desc);
      const ble_gap_upd_params params = {
          .itvl_min = BLE_GAP_CONN_ITVL_MS(50),
          .itvl_max = BLE_GAP_CONN_ITVL_MS(100),
          .latency = 10,
          .supervision_timeout =
              std::max((uint16_t)BLE_GAP_SUPERVISION_TIMEOUT_MS(2000),
                       conn_desc.supervision_timeout),
          .min_ce_len = 0,
          .max_ce_len = 0};
      BLE_ERROR_CHECK(
          ble_gap_update_params(event->connect.conn_handle, &params));
    } else {
      start_advertising();
    }
    break;
  }

  case BLE_GAP_EVENT_DISCONNECT: {
    ESP_LOGI(TAG, "Disconnected from peer; reason: %d",
             event->disconnect.reason);
    reset_gatt_subscriptions();
    start_advertising();
    break;
  }

  case BLE_GAP_EVENT_CONN_UPDATE: {
    ble_gap_conn_desc conn_desc;
    ESP_LOGI(TAG, "Connection updated; status: %d", event->conn_update.status);
    BLE_ERROR_CHECK(
        ble_gap_conn_find(event->conn_update.conn_handle, &conn_desc));
    print_conn_desc(&conn_desc);
    break;
  }

  case BLE_GAP_EVENT_ADV_COMPLETE: {
    ESP_LOGI(TAG, "Advertising completed; reason: %d",
             event->adv_complete.reason);
    start_advertising();
    break;
  }

  case BLE_GAP_EVENT_NOTIFY_TX: {
    if ((event->notify_tx.status != 0) &&
        (event->notify_tx.status != BLE_HS_EDONE)) {
      ESP_LOGI(TAG,
               "Notification error; conn_handle: %" PRIu16
               ", attr_handle: %" PRIu16 ", status: %d, is_indication: %d",
               event->notify_tx.conn_handle, event->notify_tx.attr_handle,
               event->notify_tx.status, event->notify_tx.indication);
    }
    break;
  }

  case BLE_GAP_EVENT_SUBSCRIBE: {
    ESP_LOGI(TAG,
             "Subscribed; conn_handle: %" PRIu16 ", attr_handle: %" PRIu16
             ", reason: %" PRIu8 ", prevn: %d, curn: %d, previ: %d, curi=%d",
             event->subscribe.conn_handle, event->subscribe.attr_handle,
             event->subscribe.reason, event->subscribe.prev_notify,
             event->subscribe.cur_notify, event->subscribe.prev_indicate,
             event->subscribe.cur_indicate);
    subscribe_gatt(event->subscribe);
    break;
  }

  case BLE_GAP_EVENT_MTU: {
    ESP_LOGI(TAG,
             "MTU updated, conn_handle: %" PRIu16 ", cid: %" PRIu16
             ", mtu: %" PRIu16,
             event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
    break;
  }
  }
  return 0;
}

void CommStrap::init_gap() {
  ble_svc_gap_init();
  BLE_ERROR_CHECK(ble_svc_gap_device_name_set(device_name));
}

void CommStrap::start_gap() { start_advertising(); }
