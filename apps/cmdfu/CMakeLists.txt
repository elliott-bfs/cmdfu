project(cmdfu VERSION 0.2.0 LANGUAGES C)
add_executable(cmdfu main.c)

# Create version.h file. The version is set by the project() command.
configure_file("./version.h.in" "${CMAKE_CURRENT_BINARY_DIR}/version.h")

target_include_directories(cmdfu PUBLIC "${PROJECT_DIR}/include")
# Need to add the build directory where we have placed the version.h
target_include_directories(cmdfu PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(cmdfu PRIVATE mdfulib toolslib transportlib maclib utilslib)

# Specify the installation rules
install(TARGETS cmdfu
    RUNTIME DESTINATION bin  # For Windows and Unix-like systems
)
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Microchip")
set(CPACK_PACKAGE_NAME "mdfu-tools")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Microchip Device Firmware Update (MDFU) tools. Includes
	  the MDFU host command line tool cmdfu."
)
set(CPACK_SOURCE_IGNORE_FILES
    "/build/;"
    "/test/;"
    "/cicd/;"
    "/\\\\.git/;"
    "/\\\\.vscode/;"
)
set(CPACK_GENERATOR "TGZ;DEB")
set(CPACK_SOURCE_GENERATOR "TBZ2")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Microchip <support@microchip.com>")
set(CPACK_DEBIAN_PACKAGE_SECTION "admin")

include(CPack)
