#include "lmic_hal/boards.hh"
#include <driver/gpio.h>

// Taken fom:
// https://github.com/espressif/arduino-esp32/blob/master/variants/heltec_wifi_lora_32_V2/pins_arduino.h
#define SS GPIO_NUM_18
#define SCK GPIO_NUM_5
#define MOSI GPIO_NUM_27
#define MISO GPIO_NUM_19
#define RST_LoRa GPIO_NUM_14
#define DIO1 GPIO_NUM_26
#define DIO2 GPIO_NUM_35
#define DIO3 GPIO_NUM_34

namespace ESP_IDF_LMIC {

constexpr pin_t PIN_SX1276_NSS = SS;
constexpr pin_t PIN_SX1276_NRESET = RST_LoRa;
constexpr pin_t PIN_SX1276_BUSY = UNUSED_PIN;
constexpr pin_t PIN_SX1276_DIO1 = DIO1;
constexpr pin_t PIN_SX1276_DIO2 = DIO2;
constexpr pin_t PIN_SX1276_DIO3 = DIO3;
constexpr pin_t PIN_SX1276_SPI_SCK = SCK;
constexpr pin_t PIN_SX1276_SPI_MOSI = MOSI;
constexpr pin_t PIN_SX1276_SPI_MISO = MISO;
constexpr pin_t PIN_SX1276_ANT_SWITCH_RX = UNUSED_PIN;
constexpr pin_t PIN_SX1276_ANT_SWITCH_TX_BOOST = UNUSED_PIN;
constexpr pin_t PIN_SX1276_ANT_SWITCH_TX_RFO = UNUSED_PIN;
constexpr pin_t PIN_VDD_BOOST_ENABLE = UNUSED_PIN;

static const HalBoardConfig config = {
    .nss = PIN_SX1276_NSS,
    .rxtx = PIN_SX1276_ANT_SWITCH_RX,
    .rst = PIN_SX1276_NRESET,
    .dio =
        {
            PIN_SX1276_DIO1,
            PIN_SX1276_DIO2,
            PIN_SX1276_DIO3,
        },
    .spi_sck = PIN_SX1276_SPI_SCK,
    .spi_mosi = PIN_SX1276_SPI_MOSI,
    .spi_miso = PIN_SX1276_SPI_MISO,
    .busy = PIN_SX1276_BUSY,
    .rxtx_rx_active = 0,
    .rssi_cal = 10,
    .spi_freq = 8000000, /* 8MHz */
    .flags =
        {
            .using_tcxo = false,
            .using_dcdc = false,
            .using_dio2_as_rf_switch = false,
            .using_dio3_as_tcxo_switch = false,
        },
    .callbacks = &HalBoardCallbacks::instance};

const HalBoardConfig *get_board_config_heltec_lora32_v2(void) {
  return &config;
}

}; // namespace ESP_IDF_LMIC
