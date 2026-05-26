#pragma once

#include <optional>
#include <cstdint>

#define PDPASS_TO_ERR_OK(x) ((x) == pdPASS ? ESP_OK : ESP_FAIL)
#define PD_ERROR_CHECK(x) ESP_ERROR_CHECK(PDPASS_TO_ERR_OK(x))

struct CommPatientData {
  std::optional<uint8_t> heart_rate;
  std::optional<uint8_t> spo2;
};

constexpr const char *TAG = "Comm";

#define BLE_TO_ESP_ERR(x) ((x) == 0 ? ESP_OK : ESP_FAIL)
#define BLE_ERROR_CHECK_RETURN(x, ret)                                         \
  do {                                                                         \
    const auto res = (x);                                                      \
    if (res != 0) {                                                            \
      ESP_LOGE(TAG, "BLE failed at %s:%u, code %u", __FILE__, __LINE__, res);  \
      return ret;                                                              \
    }                                                                          \
  } while (0)
#define BLE_ERROR_CHECK(x)                                                     \
  do {                                                                         \
    const auto res = (x);                                                      \
    if (res != 0) {                                                            \
      ESP_LOGE(TAG, "BLE failed at %s:%u, code %u", __FILE__, __LINE__, res);  \
    }                                                                          \
    ESP_ERROR_CHECK(BLE_TO_ESP_ERR(res));                                      \
  } while (0)

#define PRINT_ADDR "%02X:%02X:%02X:%02X:%02X:%02X"
#define PRINT_ADDR_VALS(addr)                                                  \
  (addr)[5], (addr)[4], (addr)[3], (addr)[2], (addr)[1], (addr)[0]
