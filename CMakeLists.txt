cmake_minimum_required(VERSION 3.12)

project(MDFU_Tools)

include(CTest)
enable_testing()

#[[
# This section contains options to define a location for the Linux kernel
# headers if they are not located in the "default" place where the compiler
# toolchain would find them.

# Find the kernel release version
execute_process(
  COMMAND uname -r
  OUTPUT_VARIABLE KERNEL_RELEASE
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Set the path to the kernel header files based on "uname -r" command
set(KERNEL_HEADERS_DIR "/usr/src/linux-headers-${KERNEL_RELEASE}")

# Optionally set path manually (not portable)
# set(KERNEL_HEADERS_DIR "/usr/include/linux)

# Verify that the kernel headers directory exists
if(NOT EXISTS "${KERNEL_HEADERS_DIR}")
  message(FATAL_ERROR "Kernel headers not found in ${KERNEL_HEADERS_DIR}")
endif()

# Include the kernel header files
include_directories("${KERNEL_HEADERS_DIR}/include")
]]

# C23 -> 23, C17 -> 17, C99 -> 99
set_property(GLOBAL PROPERTY C_STANDARD 23)
set_property(GLOBAL PROPERTY C_EXTENSIONS True)

option(WITH_TOOL_I2C "Build with I2C tools" ON)
option(WITH_TOOL_SPI "Build with SPI tools" ON)
option(WITH_TOOL_NETWORK "Build with Network tools" ON)
option(WITH_TOOL_SERIAL "Build with Serial tools" ON)

if (WITH_TOOL_I2C)
  add_compile_definitions(USE_TOOL_I2C)
endif()
if (WITH_TOOL_SPI)
  add_compile_definitions(USE_TOOL_SPI)
endif()
if (WITH_TOOL_NETWORK)
  add_compile_definitions(USE_TOOL_NETWORK)
endif()
if (WITH_TOOL_SERIAL)
  add_compile_definitions(USE_TOOL_SERIAL)
endif()

add_subdirectory(apps)
add_subdirectory(src)