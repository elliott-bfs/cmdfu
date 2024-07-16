set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/logging.h"
    "${PROJECT_SOURCE_DIR}/include/mdfu/image_reader.h"
    "${PROJECT_SOURCE_DIR}/include/mdfu/timeout.h"
)

add_library(utilslib logging.c timeout.c image_reader.c ${HEADER_LIST})
target_include_directories(utilslib PUBLIC "${PROJECT_SOURCE_DIR}/include")