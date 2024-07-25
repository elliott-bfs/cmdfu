project(transport_example LANGUAGES C)

add_executable(transport_example test_examples.c)

target_include_directories(transport_example PUBLIC "${PROJECT_DIR}/include")

target_link_libraries(transport_example PUBLIC mdfulib toolslib transportlib maclib utilslib)
