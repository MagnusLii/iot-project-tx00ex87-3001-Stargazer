#pragma once

#include <cmath>
#include <utility>
#include <iostream>
#include <vector>
#include <pico/stdlib.h>
#include <pico/util/datetime.h>


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

struct Coordinates {
    double latitude;
    double longitude;
    bool status;
};

struct rect_coordinates {
    double x;
    double y;
    double z;
};

struct spherical_coordinates {
    double RA;
    double DECL;
    double distance=1.0;
};

class Celestial {
    public:
    protected:
        std::vector<Coordinates> coord_table; // Right ascension and Declination
        double eccentric_anomaly(double e, double M);
        double true_anomaly(double e, double E);
        double distance(double e, double E);
};

class Moon : public Celestial {
    public:
        Moon();
    private:
        double true_anomaly(double e, double E);
        double distance(double e, double E);
};

struct orbital_elements {
    orbital_elements(double J2000_day, Planets planet);
    double N; // longitude of the ascending node
    double i; // inclination to the ecliptic (plane of the Earth's orbit)
    double w; // argument of perihelion
    double a; // semi-major axis, or mean distance from Sun
    double e; // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    double M; // mean anomaly (0 at perihelion; increases uniformly with time)
};


double normalize_degrees(double degrees);
double normalize_radians(double radians);
double datetime_to_j2000_day(datetime_t &date);
double local_sidereal_time(double J2000_day, double longitude);
double obliquity_of_eplectic(double J2000_day);
rect_coordinates to_rectangular_coordinates(spherical_coordinates sp);
spherical_coordinates to_spherical_coordinates(rect_coordinates rc);