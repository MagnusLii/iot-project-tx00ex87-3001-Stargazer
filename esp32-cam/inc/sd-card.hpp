#ifndef SD_CARD_HPP
#define SD_CARD_HPP

#include <string>
#include "esp_err.h"

class SDcard {
  public:
    SDcard(std::string mount_point_arg, int max_open_files = 2, int CMD = 15, int D0 = 2,
           int D1 = 4, int D2 = 12, int D3 = 13);

  private:
    esp_err_t sd_card_status;
};

#endif