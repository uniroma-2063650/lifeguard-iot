set(
    srcs
    "src/ina219.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES esp_driver_i2c
)
