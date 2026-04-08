set(
    srcs
    "main.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ina219 alert comm_hub esp_driver_i2c esp_driver_gpio monitor
)
