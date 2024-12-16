#include "sd-card.hpp"
#include <string>
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "debug.hpp"

SDcard::SDcard(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2, int D3) {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                     .max_files = max_open_files,
                                                     .allocation_unit_size = 16 * 1024,
                                                     .disk_status_check_enable = false,
                                                     .use_one_fat = false
                                                    };
    sdmmc_card_t *card;

    const char* mount_point = mount_point_arg.c_str();

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode((gpio_num_t)CMD, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)D0, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode((gpio_num_t)D1, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)D2, GPIO_PULLUP_ONLY);  // D2, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)D3, GPIO_PULLUP_ONLY);  // D3, needed in 4- and 1-line modes

    sd_card_status = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (sd_card_status != ESP_OK) {
        DEBUG("Failed to mount SD card");
        // TODO: Handle error
    }

    return;
}
