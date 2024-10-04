set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/tools.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/network.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/serial.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/spidev.h"
)

add_library(toolslib tools.c network_tool.c serial_tool.c spidev_tool.c ${HEADER_LIST})
target_include_directories(toolslib PUBLIC "${CMAKE_SOURCE_DIR}/include")
