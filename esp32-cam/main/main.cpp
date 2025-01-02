#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "TaskPriorities.hpp"
#include "camera.hpp"
#include "debug.hpp"
#include "requestHandler.hpp"
#include "sd-card.hpp"
#include "testMacros.hpp"
#include "wireless.hpp"
#include <memory>

#include "esp_sntp.h"
#include "tasks.hpp"
#include "timesync-lib.hpp"

#include "debug-functions.hpp"

extern "C" {
void app_main(void);
}
void app_main(void) {
    DEBUG("Starting main");

    xTaskCreate(init_task, "init-task", 4096,
                NULL, TaskPriorities::HIGH, NULL);

    // // while (1){
    // //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // //     DEBUG("Still running\n");
    // // }

    // WirelessHandler wifi;
    // wifi.connect(WIFI_SSID, WIFI_PASSWORD);
    // while (!wifi.isConnected()) {
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    // SDcard sdcard("/sdcard");
    // RequestHandler requestHandler(WEB_SERVER, WEB_PORT, WEB_PATH, std::make_shared<WirelessHandler>(wifi),
                                //   std::make_shared<SDcard>(sdcard));
    // std::string request;
    // createTestGETRequest(&request);

    // send_request_and_enqueue_response(&requestHandler, request);

    // set_tz();

    // char time[20];
    // if (get_localtime_string(time, 20) == timeSyncLibReturnCodes::SUCCESS){
    //     DEBUG(time);
    // }
    // print_free_psram();
    
    // Camera cameraPtr(std::make_shared<SDcard>(sdcard), requestHandler.getWebSrvRequestQueue());



    // xTaskCreate(take_picture_and_save_to_sdcard_in_loop_task, "take_picture_and_save_to_sdcard_in_loop_task", 4096,
                // (void *)&cameraPtr, TaskPriorities::ABSOLUTE, NULL);

    // xTaskCreate(http_get_task, "http_get_task", 4096, NULL, TaskPriorities::LOW, NULL);
    // while (1)
    // {
    //     vTaskDelay(1000 / portTICK_PERIOD_MS);
    // }

    while (1) {
        // DEBUG("app_main endless catch");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}