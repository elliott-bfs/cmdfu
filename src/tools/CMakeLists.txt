set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/tools.h"
)

add_library(toolslib tools.c ${HEADER_LIST})
target_include_directories(toolslib PUBLIC "${PROJECT_SOURCE_DIR}/include")