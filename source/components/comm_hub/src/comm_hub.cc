#include "comm_hub.hh"
#include "comm_common_priv.hh"
#include "lmic/lmic.h"

constexpr const TickType_t QUEUE_TIMEOUT = pdMS_TO_TICKS(100);

static inline LMIC_SecureElement_EUI_t DEV_EUI{
    .bytes = {'L', 'F', 'G', 'D', '_', 'H', 'U', 'B'}};

void os_getDevEui(u1_t *buf) {
  os_copyMem(buf, DEV_EUI.bytes, sizeof(LMIC_SecureElement_EUI_t));
}
void os_getArtEui(u1_t *buf) {
  os_copyMem(buf, APP_EUI.bytes, sizeof(LMIC_SecureElement_EUI_t));
}
void os_getDevKey(u1_t *buf) {
  os_copyMem(buf, APP_KEY.bytes, sizeof(LMIC_SecureElement_Aes128Key_t));
}

CommHub CommHub::instance{};

void CommHub::start(bool is_heltec_v3, const BaseType_t core,
                    const uint8_t room) {
  this->room = room;
  Comm::start(is_heltec_v3, core, CommHub::run);
}

void CommHub::run(Comm *args) {
  CommHub *self = (CommHub *)args;
  self->start_in_task();

  ESP_LOGI(TAG, "Registering event callback...");

  LMIC_registerEventCb(CommHub::process_event, nullptr);
  LMIC_registerRxMessageCb(CommHub::process_rx, nullptr);

  ESP_LOGI(TAG, "Starting joining...");

  LMIC_startJoining();

  ESP_LOGI(TAG, "Started");

  for (;;) {
    os_runloop_once();
  }

  vTaskDelete(nullptr);
}

void CommHub::process_event(void *args, ev_t event) {
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

void CommHub::process_rx(void *args, uint8_t port, const uint8_t *msg,
                         size_t msg_size) {
  ESP_LOGI(TAG, "Received message of size %zu at port %" PRIu8, msg_size, port);
  if (msg_size != CommPacket::PACKED_SIZE) {
    ESP_LOGW(TAG, "Unrecognized packet size");
    return;
  }
  CommPacket packet = CommPacket::unpack(*(const uint32_t *)msg);
  if (packet.room != instance.room) {
    ESP_LOGI(TAG, "Packet targeted at a different room (%u), ignoring",
             packet.room);
    return;
  }
  xQueueSend(instance.queue, &packet, QUEUE_TIMEOUT);
}

std::optional<CommPacket> CommHub::receivePacket(TickType_t timeout) {
  MaybeUninit<CommPacket> packet;
  return xQueueReceive(queue, &packet, timeout) == pdTRUE
             ? packet.value
             : std::optional<CommPacket>{};
}
