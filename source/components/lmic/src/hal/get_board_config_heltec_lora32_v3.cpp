#include "lmic_hal/boards.hh"
#include <driver/gpio.h>

// Taken from:
// https://github.com/espressif/arduino-esp32/blob/master/variants/heltec_wifi_lora_32_V3/pins_arduino.h
#define SS GPIO_NUM_8
#define SCK GPIO_NUM_9
#define MOSI GPIO_NUM_10
#define MISO GPIO_NUM_11
#define RST_LoRa GPIO_NUM_12
#define BUSY_LoRa GPIO_NUM_13
#define DIO1 GPIO_NUM_14

namespace ESP_IDF_LMIC {

constexpr pin_t PIN_SX1262_NSS = SS;
constexpr pin_t PIN_SX1262_NRESET = RST_LoRa;
constexpr pin_t PIN_SX1262_BUSY = BUSY_LoRa;
constexpr pin_t PIN_SX1262_DIO1 = DIO1;
constexpr pin_t PIN_SX1262_DIO2 = UNUSED_PIN;
constexpr pin_t PIN_SX1262_DIO3 = UNUSED_PIN;
constexpr pin_t PIN_SX1262_SPI_SCK = SCK;
constexpr pin_t PIN_SX1262_SPI_MOSI = MOSI;
constexpr pin_t PIN_SX1262_SPI_MISO = MISO;
constexpr pin_t PIN_SX1262_ANT_SWITCH_RX = UNUSED_PIN;
constexpr pin_t PIN_SX1262_ANT_SWITCH_TX_BOOST = UNUSED_PIN;
constexpr pin_t PIN_SX1262_ANT_SWITCH_TX_RFO = UNUSED_PIN;
constexpr pin_t PIN_VDD_BOOST_ENABLE = UNUSED_PIN;

static const HalBoardConfig config = {.nss = PIN_SX1262_NSS,
                                      .rxtx = PIN_SX1262_ANT_SWITCH_RX,
                                      .rst = PIN_SX1262_NRESET,
                                      .dio =
                                          {
                                              PIN_SX1262_DIO1,
                                              PIN_SX1262_DIO2,
                                              PIN_SX1262_DIO3,
                                          },
                                      .spi_sck = PIN_SX1262_SPI_SCK,
                                      .spi_mosi = PIN_SX1262_SPI_MOSI,
                                      .spi_miso = PIN_SX1262_SPI_MISO,
                                      .busy = PIN_SX1262_BUSY,
                                      .rxtx_rx_active = 0,
                                      .rssi_cal = 10,
                                      .spi_freq = 8000000, /* 8MHz */
                                      .flags =
                                          {
                                              .using_tcxo = false,
                                              .using_dcdc = true,
                                              .using_dio2_as_rf_switch = true,
                                              .using_dio3_as_tcxo_switch = true,
                                          },
                                      .callbacks =
                                          &HalBoardCallbacks::instance};

const HalBoardConfig *get_board_config_heltec_lora32_v3(void) {
  return &config;
}

}; // namespace ESP_IDF_LMIC
