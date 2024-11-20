#pragma once

#include "PicoUart.hpp"
#include <memory>

class GPS {
    public:
        GPS(std::shared_ptr<PicoUart> uart);

    private: 
        std::shared_ptr<PicoUart> uart;
};
