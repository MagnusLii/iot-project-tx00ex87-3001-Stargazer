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
    src/devices/stepper-motor.cpp
)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/stepper.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    inc
    inc/uart
    inc/devices
    ${COMMON_DIR}/inc
)

target_link_libraries(${PROJECT_NAME} 
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_pio
)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DEBUG)
ENDIF()

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

