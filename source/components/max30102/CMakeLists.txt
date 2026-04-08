set(
    srcs
    "src/max30102.cc"
    "src/maxim_hr_spo2.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES esp_driver_i2c
    PRIV_REQUIRES esp_driver_gpio
)
