#pragma once

#include "PicoUart.hpp"
#include <memory>

struct Coordinates {
    double latitude;
    double longitude;
};

class GPS {
    public:
        GPS(std::shared_ptr<PicoUart> uart);

    private: 
        double latitude;
        double longitude;
        
        std::shared_ptr<PicoUart> uart;
};
