set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/mac.h"
    "${PROJECT_SOURCE_DIR}/include/mdfu/socket_mac.h"
)

add_library(maclib socket_mac.c i2c_mac.c ${HEADER_LIST})
target_include_directories(maclib PUBLIC "${PROJECT_SOURCE_DIR}/include")