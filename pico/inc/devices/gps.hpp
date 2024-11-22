#pragma once

#include "PicoUart.hpp"
#include "pico/stdlib.h"
#include <hardware/timer.h>
#include <memory>
#include <string>
#include <sstream>

struct Coordinates {
    double latitude;
    double longitude;
};

class GPS {
    public:
        GPS(std::shared_ptr<PicoUart> uart);
        int locate_position(uint16_t timeout_s = 10);
        Coordinates get_coordinates() const;
    private:
        int parse_gpgga();
        int convert_to_decimal_deg(const std::string& value, const std::string& direction);

    private: 
        bool found;
        double latitude;
        double longitude;

        std::string gps_sentence;
        
        std::shared_ptr<PicoUart> uart;
};
