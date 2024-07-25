project(cmdfu VERSION 0.2.0 LANGUAGES C)
add_executable(cmdfu main.c)

# Create version.h file. The version is set by the project() command.
configure_file("./version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

target_include_directories(cmdfu PUBLIC "${PROJECT_DIR}/include")
# Need to add the build directory where we have placed the version.h
target_include_directories(cmdfu PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(cmdfu PRIVATE mdfulib toolslib transportlib maclib utilslib)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)