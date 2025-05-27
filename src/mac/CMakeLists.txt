if (WITH_TOOL_NETWORK)
    set(NETWORK_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/mac/socket_mac.h")
    set(NETWORK_SOURCE "socket_mac.c" "socket_packet_mac.c")
endif()
if (WITH_TOOL_SERIAL)
    set(SERIAL_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/mac/serial_mac.h")
    if (WIN32)
        set(SPI_SOURCE "serial_mac_win.c")
    else()
        set(SERIAL_SOURCE "serial_mac.c")
    endif()
endif()
if (WITH_TOOL_SPI)
    set(SPI_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/mac/spidev_mac.h")
    set(SPI_SOURCE "spidev_mac.c")
endif()
if (WITH_TOOL_I2C)
    set(I2C_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/mac/i2cdev_mac.h")
    set(I2C_SOURCE "i2cdev_mac.c")
endif()

set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/mac/mac.h"
    ${NETWORK_HEADER}
    ${SERIAL_HEADER}
    ${SPI_HEADER}
    ${I2C_HEADER}
)

set(SOURCE_LIST
    ${NETWORK_SOURCE}
    ${SERIAL_SOURCE}
    ${SPI_SOURCE}
    ${I2C_SOURCE}
)

add_library(maclib ${SOURCE_LIST} ${HEADER_LIST})
target_include_directories(maclib PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_include_directories(maclib PUBLIC "${CMAKE_BINARY_DIR}/include")
