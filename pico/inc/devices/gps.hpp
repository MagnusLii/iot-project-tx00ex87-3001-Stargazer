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
    enum class Mode {
        FULL_ON,
        STANDBY,
        ALWAYSLOCATE // Full-on/Standby
    };
    /*
    enum class StartType {
        NONE,
        FULL_COLD,
        COLD,
        WARM,
        HOT
    };
    */
  public:
    GPS(std::shared_ptr<PicoUart> uart/*, StartType start_type = StartType::NONE*/);
    int locate_position(uint16_t timeout_s = 10);
    Coordinates get_coordinates() const;

    //void set_nmea_output_frequencies(uint8_t gll = 1, uint8_t rmc = 1, uint8_t vtg = 1, uint8_t gga = 1,
    //                                 uint8_t gsa = 1, uint8_t gsv = 1);
    void set_mode(Mode mode);
    //void set_gptxt_output(bool enable = true, bool save = true);
    //void reset_system_defaults();

  private:
    int parse_gpgga();
    int parse_gpgll();
    int nmea_to_decimal_deg(const std::string &value, const std::string &direction);
    void full_on_mode();
    void standby_mode();
    void alwayslocate_mode();
    //void full_cold_start();

  private:
    double latitude;
    double longitude;
    bool status = false; // false if coordinates not found

    std::string gps_sentence;

    std::shared_ptr<PicoUart> uart;
};
