set(
    srcs
    "main.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    PRIV_REQUIRES max30102 comm_strap esp_driver_i2c esp_pm esp_driver_gpio
        monitor
)
