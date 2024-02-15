set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/transport.h"
    "${PROJECT_SOURCE_DIR}/include/mdfu/serial_transport.h"
)

add_library(transportlib transport.c serial_transport.c ${HEADER_LIST})
target_include_directories(transportlib PUBLIC "${PROJECT_SOURCE_DIR}/include")