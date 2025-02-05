#pragma once
#include "pico/stdlib.h"

struct Coordinates {
    double latitude;
    double longitude;
    bool status;
};

struct azimuthal_coordinates {
    double azimuth;
    double altitude;
};

struct Command {
    uint64_t id;
    azimuthal_coordinates coords;
    datetime_t time;
};