cmake_minimum_required(VERSION 3.20)

# Set commands need to be before include, after cmake_minimum_required.
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DENABLE_ESP_DEBUG")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_ESP_DEBUG")

set(COMMON_DIR ../common)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-stargazer C CXX ASM)
