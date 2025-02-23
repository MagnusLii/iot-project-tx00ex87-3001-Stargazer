#ifndef SD_CARD_HPP
#define SD_CARD_HPP

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdmmc_cmd.h"
#include <string>
#include <unordered_map>
#include <vector>

#define LINE_READ_BUFFER_SIZE 256
#define SD_CARD_MOUNT_POINT   "/sdcard"

enum class Settings {
    WIFI_SSID,
    WIFI_PASSWORD,
    WEB_DOMAIN,
    WEB_PORT,
    WEB_TOKEN,
    WEB_CERTIFICATE,
    CRC // Keep as final element
};

struct Sd_card_mount_settings {
    std::string mount_point;
    int max_open_files;
    int CMD;
    int D0;
    int D1;
    int D2;
    int D3;

    Sd_card_mount_settings()
        : mount_point(SD_CARD_MOUNT_POINT), max_open_files(2), CMD(15), D0(2), D1(4), D2(12), D3(13) {}
};

class SDcardHandler {
  public:
    SDcardHandler(Sd_card_mount_settings settings);

    ~SDcardHandler();
    esp_err_t mount_sd_card(Sd_card_mount_settings settings);

    std::string *get_mount_point();
    const char *get_mount_point_c_str();
    esp_err_t get_sd_card_status();

    int write_file(const char *filename, std::string data);
    int write_file(const char *filename, const char *data);
    int write_file(const char *filename, const uint8_t *data, const size_t len);
    int read_file(const char *filename, std::string &read_data_storage);
    int read_file_base64(const char *filename, std::string &read_data_storage);
    void add_crc(std::string &data);
    bool check_crc(const std::string &data);
    int save_all_settings(const std::unordered_map<Settings, std::string> settings);
    int read_all_settings(std::unordered_map<Settings, std::string> &settings);
    int save_setting(const Settings settingID, const std::string &value);
    int read_setting(const Settings settingID, std::string &value);
    uint32_t get_sdcard_free_space();
    esp_err_t unmount_sdcard();
    esp_err_t clear_sdcard();

  private:
    std::string mount_point;
    esp_err_t sd_card_status;
    SemaphoreHandle_t file_mutex;
    sdmmc_card_t *esp_sdcard;
};

#endif
