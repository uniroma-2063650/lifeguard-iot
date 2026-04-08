#include "comm_strap.hh"
#include "comm_common_priv.hh"
#include "lmic/lmic.h"
#include "lmic_hal/hal.hh"

constexpr const TickType_t QUEUE_TIMEOUT = pdMS_TO_TICKS(100);

static inline LMIC_SecureElement_EUI_t DEV_EUI{
    .bytes = {'L', 'F', 'G', 'D', '_', 'S', 'T', 'P'}};

void os_getDevEui(u1_t *buf) {
  os_copyMem(buf, DEV_EUI.bytes, sizeof(LMIC_SecureElement_EUI_t));
}
void os_getArtEui(u1_t *buf) {
  os_copyMem(buf, APP_EUI.bytes, sizeof(LMIC_SecureElement_EUI_t));
}
void os_getDevKey(u1_t *buf) {
  os_copyMem(buf, APP_KEY.bytes, sizeof(LMIC_SecureElement_Aes128Key_t));
}

CommStrap CommStrap::instance{};

void CommStrap::start(bool is_heltec_v3, const BaseType_t core) {
  Comm::start(is_heltec_v3, core, CommStrap::run);
}

void CommStrap::run(Comm *args) {
  CommStrap *self = (CommStrap *)args;
  self->start_in_task();

  ESP_LOGI(TAG, "Registering event callback...");

  LMIC_registerEventCb(CommStrap::process_event, nullptr);

  ESP_LOGI(TAG, "Starting joining...");

  LMIC_startJoining();

  ESP_LOGI(TAG, "Started");

  for (;;) {
    {
      MaybeUninit<CommPacket> packet;
      while (xQueueReceive(instance.queue, &packet, 0) == pdTRUE) {
        const uint32_t packed_packet = packet.value.pack();
        ESP_LOGI(TAG, "Sending packet (kind = %u): %" PRIx32, packet.value.kind,
                 packed_packet);
        // TODO: Need a working network for this
        // LMIC_TX_ERROR_CHECK(LMIC_setTxData2_strict(
        //     PORT, (u1_t *)&packed_packet, CommPacket::PACKED_SIZE, false));
      }
    }
    os_runloop_once();
  }

  vTaskDelete(nullptr);
}

void CommStrap::process_event(void *args, ev_t event) {
  const char *event_name = event_type_to_name(event);
  if (event_name) {
    ESP_LOGI(TAG, "Received event of type %s", event_name);
  } else {
    ESP_LOGI(TAG, "Received event of unknown type %02X", event);
  }
  switch (event) {
  case EV_JOINING:
    break;
  case EV_JOINED:
    break;
  case EV_JOIN_FAILED:
    break;
  case EV_REJOIN_FAILED:
    break;
  case EV_TXCOMPLETE:
    break;
  case EV_RXCOMPLETE:
    break;
  case EV_SCAN_TIMEOUT:
    break;
  case EV_BEACON_FOUND:
    break;
  case EV_BEACON_TRACKED:
    break;
  case EV_BEACON_MISSED:
    break;
  case EV_LOST_TSYNC:
    break;
  case EV_RESET:
    break;
  case EV_LINK_DEAD:
    break;
  case EV_LINK_ALIVE:
    break;
  case EV_SCAN_FOUND:
    break;
  case EV_TXSTART:
    break;
  case EV_TXCANCELED:
    break;
  case EV_RXSTART:
    break;
  case EV_JOIN_TXCOMPLETE:
    break;
  default: {
    ESP_LOGW(TAG, "Unknown event type: %02X", event);
    break;
  }
  }
}

void CommStrap::sendPacket(const CommPacket packet) {
  PD_ERROR_CHECK(xQueueSend(queue, &packet, QUEUE_TIMEOUT));
  ESP_IDF_LMIC::lmic_hal_wake_task();
}
