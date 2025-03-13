#pragma once

#include "hardware/timer.h"
#include "pico/stdlib.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

#include "PicoUart.hpp"
#include "structs.hpp"

/**
 * @class GPS
 * @brief Handles communication with a Quectel L80 GPS module over UART.
 */
class GPS {
  public:
    enum class Mode {
        UNKNOWN,
        FULL_ON,
        STANDBY,
        ALWAYSLOCATE // Full-on/Standby
    };

  public:
    GPS(std::shared_ptr<PicoUart> uart, bool gpgga_on = true, bool gpgll_on = true);
    int locate_position(uint16_t timeout_s = 10);
    Coordinates get_coordinates() const;
    void set_mode(Mode mode);
    Mode get_mode() const;
    void set_coordinates(double lat, double lon);

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
    Mode current_mode = Mode::UNKNOWN;
    double latitude = 0.0;
    double longitude = 0.0;
    bool status = false; // false if coordinates not found
    bool gpgga = false;
    bool gpgll = false;

    std::string gps_sentence;
    SentenceState sentence_state = SentenceState::EMPTY;

    std::shared_ptr<PicoUart> uart;
};
