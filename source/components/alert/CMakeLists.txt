set(
    srcs
    "src/alert.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    PRIV_REQUIRES esp_driver_gpio
)
