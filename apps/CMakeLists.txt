add_executable(mdfu main.c)

set_target_properties(mdfu PROPERTIES VERSION 0.1.0)
set_target_properties(mdfu PROPERTIES COMPILE_DEFINITIONS MAC_SOCKET)
# Configure version.h.in and create version.h in the build directory
configure_file(version.h.in version.h)

target_include_directories(mdfu PUBLIC "${PROJECT_DIR}/include")
target_include_directories(mdfu PUBLIC ${PROJECT_BINARY_DIR})

target_link_libraries(mdfu mdfulib maclib transportlib)

