set(
    srcs
    "src/comm_hub.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES lmic comm_common
)
