set(
    srcs
    "src/comm_strap.cc"
    "src/comm_strap_gap.cc"
    "src/comm_strap_gatt.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    PRIV_REQUIRES nvs_flash
    REQUIRES comm_common bt
)
