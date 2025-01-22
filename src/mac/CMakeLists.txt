set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/mac.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/socket_mac.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/serial_mac.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/spidev_mac.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/i2cdev_mac.h"
)

add_library(maclib socket_mac.c socket_packet_mac.c serial_mac.c i2cdev_mac.c spidev_mac.c ${HEADER_LIST})
target_include_directories(maclib PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(maclib PUBLIC "${CMAKE_BINARY_DIR}/include")
