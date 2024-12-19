cmake_minimum_required(VERSION 3.20)

set(PICO_BOARD pico)

# Include build functions from Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico-stargazer C CXX ASM)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)

set(COMMON_DIR ../common)

# Creates a pico-sdk subdirectory in our project for the libraries
pico_sdk_init()

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/uart/PicoUart.cpp
    src/uart/RingBuffer.cpp
    src/devices/gps.cpp
    src/devices/compass.cpp
    src/planet_finder/planet_finder.cpp
)

add_executable(test_planet_finder tests/planet_finder/test_planet_finder.cpp src/planet_finder/planet_finder.cpp tests/unity/unity.c)
target_link_libraries(test_planet_finder pico_stdlib hardware_rtc)
target_include_directories(test_planet_finder PRIVATE inc/planet_finder tests/unity)
target_compile_definitions(test_planet_finder PRIVATE UNITY_INCLUDE_CONFIG_H)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    inc
    inc/uart
    inc/devices
    inc/planet_finder
    ${COMMON_DIR}/inc
)

target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_rtc
)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DEBUG)
ENDIF()

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

