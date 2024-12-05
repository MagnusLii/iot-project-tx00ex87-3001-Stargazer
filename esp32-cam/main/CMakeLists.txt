file(GLOB SRCS
    "main.cpp"
    "../src/components/*.cpp"
)

file(GLOB INCLUDE_DIRS
    "../inc"
    "../inc/components"
    "../../common/inc"
)

idf_component_register(
    SRCS ${SRCS}
    INCLUDE_DIRS ${INCLUDE_DIRS}
    REQUIRES driver sdmmc esp_event fatfs nvs_flash
)
