#ifndef SD_CARD_HPP
#define SD_CARD_HPP

#include "esp_err.h"
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

enum class Settings{
  WIFI_SSID,
  WIFI_PASSWORD,
  WEB_PORT,
  WEB_TOKEN,
  CRC // Keep as final element
};

class SDcard {
  public:
    SDcard(std::string mount_point_arg, int max_open_files = 2, int CMD = 15, int D0 = 2, int D1 = 4, int D2 = 12,
           int D3 = 13);

    ~SDcard();
    esp_err_t mount_sd_card(std::string mount_point_arg, int max_open_files = 2, int CMD = 15, int D0 = 2,
           int D1 = 4, int D2 = 12, int D3 = 13);
    
    std::string *get_mount_point();
    const char *get_mount_point_c_str();
    esp_err_t get_sd_card_status();

    int write_file(const char* filename, std::string data);
    int write_file(const char* filename, const char* data);
    int write_file(const char* filename, const uint8_t* data, const size_t len);
    int read_file(const char* filename, std::string& data);
    int read_file(const char* filename, std::vector<uint8_t>& data);
    void add_crc(std::string &data);
    bool check_crc(const std::string &data);
    int save_all_settings(const std::vector<std::string> settings);
    int read_all_settings(std::vector<std::string> settings);
    int save_setting(const Settings settingID, const std::string &value);
    int read_setting(const Settings settingID, std::string &value);

  private:
    std::string mount_point;
    esp_err_t sd_card_status;
    SemaphoreHandle_t file_mutex;
};

#endif
