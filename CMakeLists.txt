cmake_minimum_required(VERSION 3.0.0)
# The version here will add the variables
# mdfu_VERSION_MAJOR, mdfu_VERSION_MINOR and mdfu_VERSION_PATCH
project(mdfu VERSION 0.1.0 LANGUAGES C)

include(CTest)
enable_testing()

#lwftp.c serial_transport.c
add_executable(mdfu main.c socket_mac.c serial_transport.c transport.c)
# we need to add the build directory because some source files will
# only be available there after configuration
target_include_directories(mdfu PUBLIC
                           "${PROJECT_BINARY_DIR}"
                           )

set_property(TARGET mdfu PROPERTY C_STANDARD 99)

# Configure version.h.in and create version.h in the build directory
configure_file(version.h.in version.h)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
