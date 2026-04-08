#include "comm_common.hh"
#include "FreeRTOSConfig.h"
#include "comm_common_priv.hh"
#include "lmic/lmic.h"
#include "lmic/lmic_eu_like.h"
#include "lmic_hal/boards.hh"
#include <driver/gpio.h>

bool Comm::is_running = false;

void Comm::start(bool is_heltec_v3, const BaseType_t core,
                 void (*run)(Comm *)) {
  this->is_heltec_v3 = is_heltec_v3;
  ESP_LOGI(TAG, "Starting task...");
  gpio_install_isr_service(0);
  xTaskCreatePinnedToCore((void (*)(void *))run, "comm", 8192, this,
                          configMAX_PRIORITIES - 1, &task, core);
  queue = xQueueCreate(8, sizeof(CommPacket));
  is_running = true;
}

void Comm::start_in_task() {
  ESP_LOGI(TAG, "Configuring...");

  const ESP_IDF_LMIC::HalConfig config =
      is_heltec_v3
          ? ESP_IDF_LMIC::HalConfig{.board =
                                        *ESP_IDF_LMIC::
                                            get_board_config_heltec_lora32_v3()}
          : ESP_IDF_LMIC::HalConfig{
                .board = *ESP_IDF_LMIC::get_board_config_heltec_lora32_v2()};
  if (!os_init_ex(&config)) {
    ESP_LOGE(TAG, "Failed to initialize LMIC");
    vTaskDelete(nullptr);
  }

  LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),
                    BAND_CENTI); // g-band
  LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK, DR_FSK),
                    BAND_MILLI); // g2-band
}
