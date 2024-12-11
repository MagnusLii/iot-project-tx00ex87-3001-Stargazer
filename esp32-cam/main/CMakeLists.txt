file(GLOB SRCS
    "main.cpp"
    "../src/*.cpp"
)

file(GLOB INCLUDE_DIRS
    "../inc"
    "../../common/inc" # Debug functions
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver sdmmc esp_event fatfs nvs_flash esp_wifi
)
