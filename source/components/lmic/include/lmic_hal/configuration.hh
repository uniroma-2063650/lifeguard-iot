#pragma once

#ifndef _esp_idf_lmic_hal_configuration_h_
#define _esp_idf_lmic_hal_configuration_h_

#include "lmic/lmic_env.h"
#include <FreeRTOS/FreeRTOS.h>
#include <cstdint>
#include <soc/gpio_num.h>

namespace ESP_IDF_LMIC {

/* these types should match the types used by the LMIC */
typedef int32_t ostime_t;

typedef gpio_num_t pin_t;

// Use this for any unused pins.
constexpr pin_t UNUSED_PIN = GPIO_NUM_NC;
constexpr int NUM_DIO = 3;

// forward reference

class HalBoardCallbacks {
public:
  HalBoardCallbacks(){};

  // these must match the constants in radio.c
  enum class TxPowerPolicy_t : uint8_t { RFO, PA_BOOST, PA_BOOST_20dBm };

  virtual ostime_t setModuleActive(bool state) {
    LMIC_API_PARAMETER(state);

    // by default, if not overridden, do nothing
    // and return 0 to indicate that the caller
    // need not delay.
    return 0;
  }

  virtual void begin(void) {}
  virtual void end(void) {}

  // compute desired transmit power policy.  HopeRF needs
  // (and previous versions of this library always chose)
  // PA_BOOST mode. So that's our default. Override this
  // for the Murata module.
  virtual TxPowerPolicy_t getTxPowerPolicy(TxPowerPolicy_t policy,
                                           int8_t requestedPower,
                                           uint32_t frequency) {
    LMIC_API_PARAMETER(policy);
    LMIC_API_PARAMETER(requestedPower);
    LMIC_API_PARAMETER(frequency);
    // default: use PA_BOOST exclusively
    return TxPowerPolicy_t::PA_BOOST;
  }

  static HalBoardCallbacks instance;
};

struct HalBoardConfig {
  /* the contents */
  pin_t nss;          // pin for select
  pin_t rxtx;         // pin for rx/tx control
  pin_t rst;          // pin for reset
  pin_t dio[NUM_DIO]; // pins for DIO0, DOI1, DIO2
  pin_t spi_sck;      // pin for SPI SCK
  pin_t spi_mosi;     // pin for SPI MOSI
  pin_t spi_miso;     // pin for SPI MISO
  pin_t busy;         // pin for BUSY
  // true if we must set rxtx for rx_active, false for tx_active
  uint8_t rxtx_rx_active; // polarity of rxtx active
  int8_t rssi_cal;        // cal in dB -- added to RSSI
                          //   measured prior to decision.
                          //   Must include noise guardband!
  uint32_t spi_freq;      // SPI freq in Hz.
  struct {
    bool using_tcxo: 1;
    bool using_dcdc: 1;
    bool using_dio2_as_rf_switch: 1;
    bool using_dio3_as_tcxo_switch: 1;
  } flags;
  HalBoardCallbacks* callbacks;
};

struct HalConfig {
  HalBoardConfig board;
};

bool lmic_hal_init_with_config(HalConfig *config);

}; // namespace ESP_IDF_LMIC

#endif
