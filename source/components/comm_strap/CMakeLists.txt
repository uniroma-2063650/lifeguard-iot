set(
    srcs
    "src/comm_strap.cc"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS include
    REQUIRES comm_common
)
