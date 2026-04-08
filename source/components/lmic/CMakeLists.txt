set(
    srcs
    "src/se/drivers/default/lmic_se_default.c"
    "src/hal/hal.cpp"
    "src/hal/get_board_config_heltec_lora32_v3.cpp"
    "src/hal/get_board_config_heltec_lora32_v2.cpp"
    "src/lmic/lmic_us915.c"
    "src/lmic/lmic.c"
    "src/lmic/lmic_in866.c"
    "src/lmic/lmic_kr920.c"
    "src/lmic/lmic_eu868.c"
    "src/lmic/lmic_compliance.c"
    "src/lmic/lmic_eu_like.c"
    "src/lmic/lmic_as923.c"
    "src/lmic/lmic_util.c"
    "src/lmic/oslmic.c"
    "src/lmic/radio_sx126x.c"
    "src/lmic/lmic_us_like.c"
    "src/lmic/lmic_au915.c"
    "src/lmic/lmic_channelshuffle.c"
    "src/lmic/radio_sx127x.c"
    "src/aes/lmic.c"
    "src/aes/ideetron/AES-128_V10.cpp"
    "src/aes/other.c"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_timer
)
target_compile_options(${COMPONENT_LIB} PUBLIC -Wno-unused-const-variable -Wno-unused-function -Wno-maybe-uninitialized)
