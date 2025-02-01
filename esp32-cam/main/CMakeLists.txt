file(GLOB SRCS
    "main.cpp"
    "../src/*.cpp"
    "../../common/src/*.cpp"
)

file(GLOB INCLUDE_DIRS
    "../inc"
    "../../common/inc"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver sdmmc esp_event fatfs nvs_flash esp_wifi esp_netif esp_driver_uart mbedtls
)
