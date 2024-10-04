# Set MDFU parameters and create mdfu_config.h file.
set(MDFU_MAX_COMMAND_DATA_LENGTH "1024")
configure_file(mdfu_config.h.in "${CMAKE_BINARY_DIR}/include/mdfu_config.h")

set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/mdfu.h"
)

add_library(mdfulib mdfu.c ${HEADER_LIST})
target_include_directories(mdfulib PUBLIC "${CMAKE_SOURCE_DIR}/include")

# Include directory for mdfu_config.h
target_include_directories(mdfulib PUBLIC "${PROJECT_BINARY_DIR}/include")
