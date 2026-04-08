set(
    srcs
    "src/comm_common.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES lmic
    PRIV_REQUIRES esp_driver_gpio
)
