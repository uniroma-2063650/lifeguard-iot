set(
    srcs
    "src/comm_hub.cc"
    "src/comm_hub_gap.cc"
    "src/comm_hub_gatt.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    PRIV_REQUIRES nvs_flash
    REQUIRES comm_common bt nimble_central_utils
)
