# Set MDFU parameters and create mdfu_config.h file.

# Check if CONFIG_VALUE is set, otherwise use a default value
if(NOT DEFINED MDFU_MAX_COMMAND_DATA_LENGTH)
    set(MDFU_MAX_COMMAND_DATA_LENGTH "1024")
endif()

# The MDFU spec version 1.0.0. requires at least 28 bytes
# For the unbuffered serial transport we need 2 additional bytes to hold the checksum
if(NOT DEFINED MDFU_MAX_RESPONSE_DATA_LENGTH)
set(MDFU_MAX_RESPONSE_DATA_LENGTH "30")
endif()

set(MDFU_PROTOCOL_VERSION_MAJOR 1)
set(MDFU_PROTOCOL_VERSION_MINOR 2)
set(MDFU_PROTOCOL_VERSION_PATCH 0)
set(MDFU_PROTOCOL_VERSION "${MDFU_PROTOCOL_VERSION_MAJOR}.${MDFU_PROTOCOL_VERSION_MINOR}.${MDFU_PROTOCOL_VERSION_PATCH}")

configure_file(mdfu_config.h.in "${CMAKE_BINARY_DIR}/include/mdfu/mdfu_config.h")

set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/mdfu.h"
)

add_library(mdfulib mdfu.c ${HEADER_LIST})
target_include_directories(mdfulib PUBLIC "${CMAKE_SOURCE_DIR}/include")

# Include directory for mdfu_config.h
target_include_directories(mdfulib PUBLIC "${CMAKE_BINARY_DIR}/include")
