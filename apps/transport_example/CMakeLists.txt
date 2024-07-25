project(transport_example LANGUAGES C)

add_executable(transport_example test_examples.c)

# Set MDFU parameters and create mdfu_config.h file.
set(MDFU_MAX_COMMAND_DATA_LENGTH "512")
configure_file(${CMAKE_SOURCE_DIR}/apps/cmdfu/mdfu_config.h.in ${CMAKE_CURRENT_BINARY_DIR}/mdfu_config.h)

target_include_directories(transport_example PUBLIC "${PROJECT_DIR}/include")
# Need to add the build directory where we have placed mdfu_config.h
target_include_directories(transport_example PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
#add_subdirectory("${CMAKE_SOURCE_DIR}/src" "${CMAKE_BINARY_DIR}/lib")# "${CMAKE_CURRENT_BINARY_DIR}/lib")
target_link_libraries(transport_example PRIVATE mdfulib toolslib transportlib maclib utilslib)
