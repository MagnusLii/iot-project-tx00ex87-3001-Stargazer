#pragma once

#include "PicoUart.hpp"
#include "pico/stdlib.h"
#include <hardware/timer.h>
#include <memory>
#include <sstream>
#include <string>

struct Coordinates {
    double latitude;
    double longitude;
    bool status;
};

class GPS {
  public:
    GPS(std::shared_ptr<PicoUart> uart);
    int locate_position(uint16_t timeout_s = 10);
    Coordinates get_coordinates() const;

  private:
    int parse_gpgga();
    int parse_gpgll();
    int nmea_to_decimal_deg(const std::string &value, const std::string &direction);

  private:
    double latitude;
    double longitude;
    bool status; // false if coordinates not found

    std::string gps_sentence;

    std::shared_ptr<PicoUart> uart;
};
