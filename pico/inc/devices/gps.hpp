#pragma once

#include "PicoUart.hpp"
#include "pico/stdlib.h"
#include "structs.hpp"
#include <hardware/timer.h>
#include <memory>
#include <sstream>
#include <string>

// struct Coordinates {
//     double latitude;
//     double longitude;
//     bool status;
// };

class GPS {
  public:
    enum class Mode {
        FULL_ON,
        STANDBY,
        ALWAYSLOCATE // Full-on/Standby
    };

  public:
    GPS(std::shared_ptr<PicoUart> uart, bool gpgga_on = true, bool gpgll_on = true);
    int locate_position(uint16_t timeout_s = 10);
    Coordinates get_coordinates() const;
    void set_mode(Mode mode);
    #ifdef ENABLE_DEBUG
    void debug_set_coordinates(double lat, double lon);
    #endif
  private:
    enum class SentenceState {
        EMPTY,
        INCOMPLETE,
        COMPLETE
    };

  private:
    int parse_output(std::string output);
    int parse_gpgga();
    int parse_gpgll();
    int nmea_to_decimal_deg(const std::string &value, const std::string &direction);
    void full_on_mode();
    void standby_mode();
    void alwayslocate_mode();

  private:
    double latitude;
    double longitude;
    bool status = false; // false if coordinates not found
    bool gpgga = false;
    bool gpgll = false;

    std::string gps_sentence;
    SentenceState sentence_state = SentenceState::EMPTY;

    std::shared_ptr<PicoUart> uart;
};
