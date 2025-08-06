if (LINUX_SUBSYSTEM_NETWORK)
    set(NETWORK_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/tools/network.h")
    set(NETWORK_SOURCE "network_tool.c")
endif()
if (LINUX_SUBSYSTEM_SERIAL OR WINDOWS_SUBSYSTEM_SERIAL)
    set(SERIAL_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/tools/serial.h")
    set(SERIAL_SOURCE "serial_tool.c")
endif()
if (LINUX_SUBSYSTEM_SPI)
    set(SPI_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/tools/spidev.h")
    set(SPI_SOURCE "spidev_tool.c")
endif()
if (LINUX_SUBSYSTEM_I2C)
    set(I2C_HEADER "${CMAKE_SOURCE_DIR}/include/mdfu/tools/i2cdev.h")
    set(I2C_SOURCE "i2cdev_tool.c")
endif()

set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/tools.h"
    ${NETWORK_HEADER}
    ${SERIAL_HEADER}
    ${SPI_HEADER}
    ${I2C_HEADER}
)

set(SOURCE_LIST
    "tools.c"
    ${NETWORK_SOURCE}
    ${SERIAL_SOURCE}
    ${SPI_SOURCE}
    ${I2C_SOURCE}
)

add_library(toolslib ${SOURCE_LIST} ${HEADER_LIST})
target_include_directories(toolslib PUBLIC "${CMAKE_SOURCE_DIR}/include")
