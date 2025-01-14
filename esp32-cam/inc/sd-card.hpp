#ifndef SD_CARD_HPP
#define SD_CARD_HPP

#include "esp_err.h"
#include <string>

enum class Settings{
  WIFI_SSID,
  WIFI_PASSWORD,
  WEB_PATH,
  WEB_PORT,
  WEB_TOKEN,
};

class SDcard {
  public:
    SDcard(std::string mount_point_arg, int max_open_files = 2, int CMD = 15, int D0 = 2, int D1 = 4, int D2 = 12,
           int D3 = 13);
    esp_err_t mount_sd_card(std::string mount_point_arg, int max_open_files = 2, int CMD = 15, int D0 = 2,
           int D1 = 4, int D2 = 12, int D3 = 13);
    
    std::string *get_mount_point();
    const char *get_mount_point_c_str();
    esp_err_t get_sd_card_status();

    void write_file(const char* filename, std::string data);
    void add_crc(std::string &data);
    bool check_crc(std::string &data);
    void save_all_settings();
    void read_all_settings();
    void save_setting(Settings settingID, std::string value);
    void read_setting(Settings settingID, std::string &value);

  private:
    std::string mount_point;
    esp_err_t sd_card_status;
};

#endif