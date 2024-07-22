set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/transport.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/serial_transport.h"
)

add_library(transportlib transport.c serial_transport.c ${HEADER_LIST})
target_include_directories(transportlib PUBLIC "${CMAKE_SOURCE_DIR}/include")

target_include_directories(transportlib PUBLIC ${PROJECT_BINARY_DIR})
