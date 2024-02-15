set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/logging.h"
)

add_library(utilslib logging.c ${HEADER_LIST})
target_include_directories(utilslib PUBLIC "${PROJECT_SOURCE_DIR}/include")