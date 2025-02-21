# Set root dir var
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

file(GLOB_RECURSE SRC_FILES "${ROOT_DIR}/src/*.c"
                            "${ROOT_DIR}/src/*.cpp"
                            "${ROOT_DIR}/../common/src/*.cpp")

file(GLOB INCLUDE_DIRS
    "${ROOT_DIR}/inc"
    "${ROOT_DIR}/../common/inc"
    "${ROOT_DIR}/inc/test-cases"
)


idf_component_register(SRCS "unity_tests.cpp" ${SRC_FILES}
                       INCLUDE_DIRS ${INCLUDE_DIRS}
                       REQUIRES unity driver sdmmc esp_event fatfs nvs_flash esp_wifi esp_netif esp_driver_uart mbedtls)
