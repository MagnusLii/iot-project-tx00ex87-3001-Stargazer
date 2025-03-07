#pragma once

#include <cmath>
#include <utility>
#include <iostream>
#include <vector>
#include "pico/stdlib.h"
#include "pico/util/datetime.h"
#include "debug.hpp"
#include "date_utils.hpp"
#include "gps.hpp"
#include "structs.hpp"

#define TABLE_LEN 24


enum Planets {
    SUN=1,
    MOON,
    MERCURY,
    VENUS,
    MARS,
    JUPITER,
    SATURN,
    URANUS,
    NEPTUNE
};

enum Interest_point {
    ASCENDING=1,
    ZENITH,
    DESCENDING,
    ABOVE,
    BELOW
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

// struct azimuthal_coordinates {
//     double azimuth;
//     double altitude;
// };


struct orbital_elements {
    orbital_elements(double J2000_day, Planets planet);
    double N; // longitude of the ascending node
    double i; // inclination to the ecliptic (plane of the Earth's orbit)
    double w; // argument of perihelion
    double a; // semi-major axis, or mean distance from Sun
    double e; // eccentricity (0=circle, 0-1=ellipse, 1=parabola)
    double M; // mean anomaly (0 at perihelion; increases uniformly with time)
};


class Celestial {
    public:
        Celestial(Planets planet);
        azimuthal_coordinates get_coordinates(const datetime_t &date);
        // void fill_coordinate_table(datetime_t date, const Coordinates observer_coordinates);
        void print_coordinates(const datetime_t start_date, int hours);
        Command get_interest_point_command(Interest_point point,const datetime_t &start_date);
        void set_observer_coordinates(const Coordinates observer_coordinates);
        void start_trace(datetime_t start_datetime, int hours);
        Command next_trace(void);
        int get_planet(void);
        void print_planet(void);
    private:
        bool check_for_above_horizon(const azimuthal_coordinates &current);
        bool check_for_rising(const azimuthal_coordinates &current, const azimuthal_coordinates &next);
        bool check_for_falling(const azimuthal_coordinates &current, const azimuthal_coordinates &next);
        bool check_for_zenith(const azimuthal_coordinates & last, const azimuthal_coordinates &current, const azimuthal_coordinates &next);
        std::vector<Command> get_interesting_commands(const datetime_t &start_date);
        Command get_zenith_time(const datetime_t &start_date);
        Planets planet;
        Coordinates observer_coordinates;
        datetime_t trace_date;
        int trace_hours;
        // azimuthal_coordinates coordinate_table[TABLE_LEN];

};


double eccentric_anomaly(double e, double M);
double true_anomaly(rect_coordinates coords);
double distance(rect_coordinates coords);
ecliptic_coordinates perturbation_moon(const orbital_elements &moon, const orbital_elements &sun);
ecliptic_coordinates perturbation_jupiter(double Mj, double Ms);
ecliptic_coordinates perturbation_saturn(double Mj, double Ms);
ecliptic_coordinates perturbation_uranus(double Mu, double Mj, double Ms);

double normalize_degrees(double degrees);
double normalize_radians(double radians);
double datetime_to_j2000_day(const datetime_t &date);
double local_sidereal_time(double J2000_day, double longitude);
double obliquity_of_eplectic(double J2000_day);
rect_coordinates rotate_through_obliquity_of_eplectic(const rect_coordinates &rc, double obliquity);
rect_coordinates to_rectangular_coordinates(spherical_coordinates sp);
rect_coordinates to_rectangular_coordinates(ecliptic_coordinates ec);
rect_coordinates to_rectangular_coordinates(double a, double e, double E);
rect_coordinates to_rectangular_coordinates(double N, double i, double w, double v, double r);
spherical_coordinates to_spherical_coordinates(rect_coordinates rc);
ecliptic_coordinates to_ecliptic_coordinates(rect_coordinates rc);
