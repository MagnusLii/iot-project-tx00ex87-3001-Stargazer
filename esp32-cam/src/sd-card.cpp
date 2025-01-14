#include "sd-card.hpp"
#include "debug.hpp"
#include "driver/sdmmc_defs.h"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <string>
#include "crc.hpp"

SDcard::SDcard(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2, int D3) {
    for (int i = 0; i < 3; i++) {
        this->sd_card_status = this->mount_sd_card(mount_point_arg, max_open_files, CMD, D0, D1, D2, D3);
        if (this->sd_card_status == ESP_OK) {
            DEBUG("SD card mounted at ", mount_point);
            return;
        }
        DEBUG("SD card mount failed, attempt ", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    DEBUG("Failed to mount SD card after retries, error: ", esp_err_to_name(sd_card_status));
    this->sd_card_status = ESP_FAIL;
    return;
}

esp_err_t SDcard::mount_sd_card(std::string mount_point_arg, int max_open_files, int CMD, int D0, int D1, int D2,
                                int D3) {

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {.format_if_mount_failed = false,
                                                     .max_files = max_open_files,
                                                     .allocation_unit_size = 16 * 1024,
                                                     .disk_status_check_enable = false,
                                                     .use_one_fat = false};
    sdmmc_card_t *card;

    const char *mount_point = mount_point_arg.c_str();
    this->mount_point = mount_point_arg;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    gpio_set_pull_mode((gpio_num_t)CMD, GPIO_PULLUP_ONLY); // CMD, needed in 4- and 1- line modes
    gpio_set_pull_mode((gpio_num_t)D0, GPIO_PULLUP_ONLY);  // D0, needed in 4- and 1-line modes
    gpio_set_pull_mode((gpio_num_t)D1, GPIO_PULLUP_ONLY);  // D1, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)D2, GPIO_PULLUP_ONLY);  // D2, needed in 4-line mode only
    gpio_set_pull_mode((gpio_num_t)D3, GPIO_PULLUP_ONLY);  // D3, needed in 4- and 1-line modes

    return esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
}
std::string *SDcard::get_mount_point() { return &this->mount_point; }
const char *SDcard::get_mount_point_c_str() { return this->mount_point.c_str(); }
esp_err_t SDcard::get_sd_card_status() { return this->sd_card_status; }

//  filename is expected to contain file extension
void SDcard::write_file(const char *filename, std::string data) {
    std::string full_filename_str = this->mount_point + "/" + filename;

    FILE *file = fopen(full_filename_str.c_str(), "w");

    DEBUG("Writing picture to ", full_filename_str);
    if (file) {
        if (fwrite(data.c_str(), 1, data.length(), file) == data.length()) {
            DEBUG("File write success");
            fclose(file);
        } else {
            DEBUG("File write failed");
        }
    } else {
        DEBUG("Failed to open file for writing");
        perror("File open error");
    }
    fclose(file);
}

void SDcard::add_crc(std::string &data) {
    uint16_t crc = crc16(data);
    data += "," + std::to_string(crc);
}

bool SDcard::check_crc(std::string &data) {
    size_t pos = data.find_last_of(',');
    if (pos == std::string::npos) {
        return false;
    }
    std::string crc_str = data.substr(pos + 1);
    uint16_t crc = std::stoi(crc_str);
    data = data.substr(0, pos);

    return (crc == crc16(data));
}