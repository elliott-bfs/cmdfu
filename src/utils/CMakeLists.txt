set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/logging.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/image_reader.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/timeout.h"
)

add_library(utilslib logging.c timeout.c image_reader.c ${HEADER_LIST})
target_include_directories(utilslib PUBLIC "${CMAKE_SOURCE_DIR}/include")