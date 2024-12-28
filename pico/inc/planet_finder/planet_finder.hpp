#pragma once

#include <cmath>
#include <utility>
#include <iostream>
#include <vector>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "debug.hpp"


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
    rect_coordinates operator+(const rect_coordinates ob) {
        rect_coordinates result;
        result.x = x + ob.x;
        result.y = y + ob.y;
        result.z = z + ob.z;
        return result;
    }
};

struct spherical_coordinates {
    double RA;
    double DECL;
    double distance=1.0;
};

struct ecliptic_coordinates {
    double lat;
    double lon;
    double distance=1.0;
    ecliptic_coordinates operator+(const ecliptic_coordinates ob) {
        ecliptic_coordinates result;
        result.lat = lat + ob.lat;
        result.lon = lon + ob.lon;
        result.distance = distance + ob.distance;
        return result;
    }
};

struct azimuthal_coordinates {
    double azimuth;
    double altitude;
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


class Sun {
    public:
        Sun(double J2000_day);
        orbital_elements oe;
        double mean_longitude(void);
        ecliptic_coordinates get_ecliptic_coordinates(void);
};


class Celestial {
    public:
        Celestial(Planets planet);
        azimuthal_coordinates get_coordinates(datetime_t &date, Coordinates observer_coordinates);
    private:
        Planets planet;
        std::vector<Coordinates> coord_table; // Right ascension and Declination
};



double sun_true_longitude(double sun_v, double sun_w);
double eccentric_anomaly(double e, double M);
double true_anomaly(rect_coordinates coords);
double distance(rect_coordinates coords);
ecliptic_coordinates perturbation_moon(const orbital_elements &moon, const orbital_elements &sun);
ecliptic_coordinates perturbation_jupiter(double Mj, double Ms);
ecliptic_coordinates perturbation_saturn(double Mj, double Ms);
ecliptic_coordinates perturbation_uranus(double Mu, double Mj, double Ms);

double normalize_degrees(double degrees);
double normalize_radians(double radians);
double datetime_to_j2000_day(datetime_t &date);
double local_sidereal_time(double J2000_day, double longitude);
double obliquity_of_eplectic(double J2000_day);
rect_coordinates rotate_through_obliquity_of_eplectic(const rect_coordinates &rc, double obliquity);
rect_coordinates to_rectangular_coordinates(spherical_coordinates sp);
rect_coordinates to_rectangular_coordinates(ecliptic_coordinates ec);
rect_coordinates to_rectangular_coordinates(double a, double e, double E);
rect_coordinates to_rectangular_coordinates(double N, double i, double w, double v, double r);
spherical_coordinates to_spherical_coordinates(rect_coordinates rc);
ecliptic_coordinates to_ecliptic_coordinates(rect_coordinates rc);