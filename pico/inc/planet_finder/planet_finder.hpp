#pragma once

#include <cmath>
#include <utility>
#include <iostream>
#include <pico/stdlib.h>
#include <pico/util/datetime.h>


/*
PRIVATE
*/

enum Planets {
    SUN,
    MOON,
    MERCURY,
    VENUS,
    MARS,
    JUPITER,
    SATURN,
    URANUS,
    NEPTUNE
};

struct orbital_elements {
    orbital_elements(float J2000_day, Planets planet);
    float N; // longitude of the ascending node
    float i; // inclination to the ecliptic (plane of the Earth's orbit)
    float w; // argument of perihelion
    float a; // semi-major axis, or mean distance from Sun
    float e; // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    float M; // mean anomaly (0 at perihelion; increases uniformly with time)
    std::pair<float, float> Nc, ic, wc, ac, ec, Mc;
};

struct orbital_element_constants {
    std::pair<float, float> Nc, ic, wc, ac, ec, Mc;
};

float normalize_degrees(float degrees);
float normalize_radians(float radians);
float datetime_to_j2000_day(datetime_t &date);
float local_sidereal_time(float J2000_day, float longitude);