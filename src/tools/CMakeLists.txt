set(HEADER_LIST
    "${PROJECT_SOURCE_DIR}/include/mdfu/tools.h"
    "${PROJECT_SOURCE_DIR}/include/mdfu/tools/network.h"
)

add_library(toolslib tools.c network_tool.c ${HEADER_LIST})
target_include_directories(toolslib PUBLIC "${PROJECT_SOURCE_DIR}/include")