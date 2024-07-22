set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/socket_mac.h"
)

add_library(maclib socket_mac.c i2c_mac.c ${HEADER_LIST})
target_include_directories(maclib PUBLIC "${CMAKE_SOURCE_DIR}/include")