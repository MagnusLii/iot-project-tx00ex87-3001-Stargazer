// #define ENABLE_ESP_DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"

#include "camera.hpp"
#include "sd-card.hpp"
#include <memory>
#include "TaskPriorities.hpp"
#include "wireless.hpp"
#include "testMacros.hpp"
#include "requestHandler.hpp"
#include "debug.hpp"

extern "C" {
    void app_main(void);
}
void app_main(void){
    // xTaskCreate(take_picture_and_save_to_sdcard_in_loop_task, "take_picture_and_save_to_sdcard_in_loop_task", 4096, NULL, TaskPriorities::ABSOLUTE, NULL);

    DEBUG("Starting main\n");
    // while (1){
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    //     DEBUG("Still running\n");
    // }
    

    WirelessHandler wifi;
    wifi.connect(WIFI_SSID, WIFI_PASSWORD);
    while (!wifi.isConnected())
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    xTaskCreate(http_get_task, "http_get_task", 4096, NULL, TaskPriorities::LOW, NULL);
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}