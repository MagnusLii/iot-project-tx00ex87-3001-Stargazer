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
    src/hardware/clock.cpp
    src/devices/stepper-motor.cpp
)

add_executable(${PROJECT_NAME}_test
    src/test_main.cpp
    src/uart/PicoUart.cpp
    src/uart/RingBuffer.cpp
    src/hardware/clock.cpp
    src/convert.cpp
    src/commbridge.cpp
    src/message.cpp
    src/crc.cpp
)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
pico_add_extra_outputs(${PROJECT_NAME}_test)


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    inc
    inc/uart
    inc/devices
    inc/hardware
    ${COMMON_DIR}/inc
)

target_include_directories(${PROJECT_NAME}_test PRIVATE
    src
    inc
    inc/uart
    inc/devices
    inc/hardware
    ${COMMON_DIR}/inc
)

target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_rtc
)

target_link_libraries(${PROJECT_NAME}_test 
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_rtc
)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DEBUG)
    target_compile_definitions(${PROJECT_NAME}_test PRIVATE ENABLE_DEBUG)
ENDIF()

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_enable_stdio_usb(${PROJECT_NAME}_test 1)
pico_enable_stdio_uart(${PROJECT_NAME}_test 0)
