set(HEADER_LIST
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools.h"
    "${CMAKE_SOURCE_DIR}/include/mdfu/tools/network.h"
)

add_library(toolslib tools.c network_tool.c ${HEADER_LIST})
target_include_directories(toolslib PUBLIC "${CMAKE_SOURCE_DIR}/include")