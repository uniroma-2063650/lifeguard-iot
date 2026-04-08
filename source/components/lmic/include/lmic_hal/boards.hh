#pragma once

#ifndef _esp_idf_lmic_hal_boards_h_
#define _esp_idf_lmic_hal_boards_h_

#include "configuration.hh"

namespace ESP_IDF_LMIC {

const HalBoardConfig *get_board_config_heltec_lora32_v2();
const HalBoardConfig *get_board_config_heltec_lora32_v3();

}; // namespace ESP_IDF_LMIC

#endif /* _esp_idf_lmic_hal_boards_h_ */
