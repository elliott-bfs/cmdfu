set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/transport/transport.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/transport/serial_transport.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/transport/spi_transport.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/transport/i2c_transport.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/error.h"
)

add_library(transportlib transport.c serial_transport.c serial_transport_buffered.c spi_transport.c i2c_transport.c ${HEADER_LIST})
target_include_directories(transportlib PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_include_directories(transportlib PRIVATE "${CMAKE_BINARY_DIR}/include")