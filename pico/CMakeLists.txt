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
    src/hardware/uart/PicoUart.cpp
    src/hardware/uart/RingBuffer.cpp
    src/devices/gps.cpp
    src/devices/compass.cpp
    src/hardware/clock.cpp
    src/devices/stepper-motor.cpp
)

add_executable(${PROJECT_NAME}_test
    src/test_main.cpp
    src/hardware/uart/PicoUart.cpp
    src/hardware/uart/RingBuffer.cpp
    src/hardware/clock.cpp
    src/commbridge.cpp
)

add_library(message ${COMMON_DIR}/src/message.cpp)
add_library(crc ${COMMON_DIR}/src/crc.cpp)
add_library(convert ${COMMON_DIR}/src/convert.cpp)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
pico_add_extra_outputs(${PROJECT_NAME}_test)


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    inc
    inc/devices
    inc/hardware
    inc/hardware/uart
    ${COMMON_DIR}/inc
)

target_include_directories(${PROJECT_NAME}_test PRIVATE
    src
    inc
    inc/devices
    inc/hardware
    inc/hardware/uart
    ${COMMON_DIR}/inc
)

target_include_directories(message PRIVATE ${COMMON_DIR}/inc)
target_include_directories(crc PRIVATE ${COMMON_DIR}/inc)
target_include_directories(convert PRIVATE ${COMMON_DIR}/inc)

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
    message
    crc
    convert
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
