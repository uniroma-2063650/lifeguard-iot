set(
    srcs
    "src/monitor.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES esp_lcd esp_pm
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2c
)
