set(srcs)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES bt
    PRIV_REQUIRES esp_driver_gpio
)
