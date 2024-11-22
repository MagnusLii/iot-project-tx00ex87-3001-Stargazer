#pragma once

#include "PicoUart.hpp"
#include "pico/stdlib.h"
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
        int convert_to_decimal(const std::string& degrees, const std::string& minutes, const std::string& direction);

    private: 
        double latitude;
        double longitude;

        std::string gps_sentence;
        std::string gpgga;
        
        std::shared_ptr<PicoUart> uart;
};
