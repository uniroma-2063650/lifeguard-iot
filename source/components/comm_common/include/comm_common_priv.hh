#include "lmic/lmic.h"
#include "lmic_se/lmic_secure_element_api.h"
#include <freertos/FreeRTOS.h>

constexpr const char *event_type_to_name(ev_t event) {
  switch (event) {
  case EV_SCAN_TIMEOUT:
    return "EV_SCAN_TIMEOUT";
  case EV_BEACON_FOUND:
    return "EV_BEACON_FOUND";
  case EV_BEACON_MISSED:
    return "EV_BEACON_MISSED";
  case EV_BEACON_TRACKED:
    return "EV_BEACON_TRACKED";
  case EV_JOINING:
    return "EV_JOINING";
  case EV_JOINED:
    return "EV_JOINED";
  case EV_RFU1:
    return "EV_RFU1";
  case EV_JOIN_FAILED:
    return "EV_JOIN_FAILED";
  case EV_REJOIN_FAILED:
    return "EV_REJOIN_FAILED";
  case EV_TXCOMPLETE:
    return "EV_TXCOMPLETE";
  case EV_LOST_TSYNC:
    return "EV_LOST_TSYNC";
  case EV_RESET:
    return "EV_RESET";
  case EV_RXCOMPLETE:
    return "EV_RXCOMPLETE";
  case EV_LINK_DEAD:
    return "EV_LINK_DEAD";
  case EV_LINK_ALIVE:
    return "EV_LINK_ALIVE";
  case EV_SCAN_FOUND:
    return "EV_SCAN_FOUND";
  case EV_TXSTART:
    return "EV_TXSTART";
  case EV_TXCANCELED:
    return "EV_TXCANCELED";
  case EV_RXSTART:
    return "EV_RXSTART";
  case EV_JOIN_TXCOMPLETE:
    return "EV_JOIN_TXCOMPLETE";
  default:
    return nullptr;
  }
}

constexpr const char *lmic_tx_error_desc(lmic_tx_error_t error) {
  switch (error) {
  case LMIC_ERROR_SUCCESS:
    return "success";
  case LMIC_ERROR_TX_BUSY:
    return "TX busy";
  case LMIC_ERROR_TX_TOO_LARGE:
    return "TX too large";
  case LMIC_ERROR_TX_NOT_FEASIBLE:
    return "TX not feasible";
  case LMIC_ERROR_TX_FAILED:
    return "TX failed";
  default:
    return "unknown error";
  }
}

constexpr const char *lmic_beacon_error_desc(lmic_beacon_error_t error) {
  switch (error) {
  case LMIC_BEACON_ERROR_INVALID:
    return "invalid";
  case LMIC_BEACON_ERROR_WRONG_NETWORK:
    return "wrong network";
  case LMIC_BEACON_ERROR_SUCCESS_PARTIAL:
    return "success partial";
  case LMIC_BEACON_ERROR_SUCCESS_FULL:
    return "success full";
  default:
    return "unknown error";
  }
}

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

#define LMIC_TX_ERROR_CHECK(x)                                                 \
  do {                                                                         \
    lmic_tx_error_t result = (x);                                              \
    if (result != LMIC_ERROR_SUCCESS) {                                        \
      ESP_LOGE(TAG, "TX error at %s:%d: %s", __FILE__, __LINE__,               \
               lmic_tx_error_desc(result));                                    \
      abort();                                                                 \
    }                                                                          \
  } while (0);
#define LMIC_BEACON_ERROR_CHECK(x)                                             \
  do {                                                                         \
    lmic_beacon_error_t result = (x);                                          \
    if (result != LMIC_ERROR_SUCCESS) {                                        \
      ESP_LOGE(TAG, "Beacon error at %s:%d: %s", __FILE__, __LINE__,           \
               lmic_beacon_error_desc(result));                                \
      abort();                                                                 \
    }                                                                          \
  } while (0);

constexpr const char *TAG = "Comm";

static inline LMIC_SecureElement_EUI_t APP_EUI{
    .bytes = {'L', 'F', 'G', 'D', '_', 'A', 'P', 'P'}};
static inline LMIC_SecureElement_Aes128Key_t APP_KEY{
    .bytes = {0xAA, 0x55, 0xAA, 0x55, 0x18, 0x27, 0x36, 0x45, 0x54, 0x63, 0x72,
              0x81, 0x55, 0xAA, 0x55, 0xAA}};

constexpr u1_t PORT = 'L';
