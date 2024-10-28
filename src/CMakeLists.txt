source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${HEADER_LIST}
)
add_subdirectory(mdfu)
add_subdirectory(mac)
add_subdirectory(transport)
add_subdirectory(tools)
add_subdirectory(utils)