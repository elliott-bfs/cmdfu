set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/logging.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/image_reader.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/image_writer.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/timeout.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/checksum.h"
)

add_library(utilslib logging.c timeout.c checksum.c image_reader.c image_writer.c ${HEADER_LIST})
target_include_directories(utilslib PUBLIC "${CMAKE_SOURCE_DIR}/include")