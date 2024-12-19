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
    orbital_elements(double J2000_day, Planets planet);
    double N; // longitude of the ascending node
    double i; // inclination to the ecliptic (plane of the Earth's orbit)
    double w; // argument of perihelion
    double a; // semi-major axis, or mean distance from Sun
    double e; // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    double M; // mean anomaly (0 at perihelion; increases uniformly with time)
    std::pair<double, double> Nc, ic, wc, ac, ec, Mc;
};


double normalize_degrees(double degrees);
double normalize_radians(double radians);
double datetime_to_j2000_day(datetime_t &date);
double local_sidereal_time(double J2000_day, double longitude);