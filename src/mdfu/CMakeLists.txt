set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/mdfu.h"
)

add_library(mdfulib mdfu.c ${HEADER_LIST})
target_include_directories(mdfulib PUBLIC "${PROJECT_SOURCE_DIR}/include")