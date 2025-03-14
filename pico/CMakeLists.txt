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
    src/commbridge.cpp
    src/controller.cpp
    src/hardware/uart/PicoUart.cpp
    src/hardware/uart/RingBuffer.cpp
    src/hardware/clock.cpp
    src/devices/gps.cpp
    src/devices/compass.cpp
    src/devices/stepper-motor.cpp
    src/devices/motor-control.cpp
    src/planet_finder/planet_finder.cpp
    src/planet_finder/date_utils.cpp
    src/devices/eeprom/eeprom.c
    src/devices/eeprom/storage.cpp
)

add_executable(${PROJECT_NAME}_test
    src/test_main.cpp
    src/commbridge.cpp
    src/controller.cpp
    src/hardware/uart/PicoUart.cpp
    src/hardware/uart/RingBuffer.cpp
    src/hardware/clock.cpp
    src/devices/gps.cpp
    src/devices/compass.cpp
    src/devices/stepper-motor.cpp
    src/devices/motor-control.cpp
    src/planet_finder/planet_finder.cpp
    src/planet_finder/date_utils.cpp
    src/devices/eeprom/eeprom.c
    src/devices/eeprom/storage.cpp
)

add_library(message ${COMMON_DIR}/src/message.cpp)
add_library(crc ${COMMON_DIR}/src/crc.cpp)
add_library(convert ${COMMON_DIR}/src/convert.cpp)

add_executable(test_planet_finder tests/planet_finder/printer.cpp src/planet_finder/planet_finder.cpp src/planet_finder/date_utils.cpp tests/unity/src/unity.c src/devices/gps.cpp src/hardware/uart/PicoUart.cpp src/devices/motor-control.cpp)
target_link_libraries(test_planet_finder pico_stdlib hardware_rtc hardware_pio)
target_include_directories(test_planet_finder PRIVATE inc/planet_finder inc/devices tests/unity/src tests/planet_finder ${COMMON_DIR}/inc inc inc/hardware/uart)
target_compile_definitions(test_planet_finder PRIVATE UNITY_INCLUDE_CONFIG_H)
target_compile_options(test_planet_finder PRIVATE -Wno-psabi)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/stepper.pio)
pico_generate_pio_header(${PROJECT_NAME}_test ${CMAKE_CURRENT_LIST_DIR}/stepper.pio)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})
pico_add_extra_outputs(${PROJECT_NAME}_test)


target_include_directories(${PROJECT_NAME} PRIVATE
    src
    inc
    inc/devices
    inc/devices/eeprom
    inc/hardware
    inc/hardware/uart
    inc/devices
    inc/planet_finder
    ${COMMON_DIR}/inc
)

target_include_directories(${PROJECT_NAME}_test PRIVATE
    src
    inc
    inc/devices
    inc/devices/eeprom
    inc/hardware
    inc/hardware/uart
    inc/devices
    inc/planet_finder
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
    hardware_pio
    message
    crc
    convert
)

target_link_libraries(${PROJECT_NAME}_test 
    pico_stdlib
    hardware_i2c
    hardware_gpio
    hardware_rtc
    hardware_pio
    message
    crc
    convert
)

IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_DEBUG)
    target_compile_definitions(${PROJECT_NAME}_test PRIVATE ENABLE_DEBUG)
    # target_compile_definitions(test_planet_finder PRIVATE ENABLE_DEBUG)
ENDIF()

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wno-psabi)
target_compile_options(${PROJECT_NAME}_test PRIVATE -Wall -Wno-psabi)
target_compile_options(test_planet_finder PRIVATE -Wall -Wno-psabi)

# Enable usb output, disable uart output
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

pico_enable_stdio_usb(${PROJECT_NAME}_test 1)
pico_enable_stdio_uart(${PROJECT_NAME}_test 0)

pico_enable_stdio_usb(test_planet_finder 0)
pico_enable_stdio_uart(test_planet_finder 1)
