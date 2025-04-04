#include "planet_finder.hpp"

#define ECCENTRIC_ANOMALY_APPROXXIMATION_MAX_ITER 3
#define ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR 0.001

/**
 * @brief Converts degrees to radians
 * @param degrees degrees to convert
 * @return Radians converted from degrees
 */
static inline double to_rads(double degrees) {
    return degrees * M_PI / 180.0;
}
/**
 * @brief Converts radians to degrees
 * @param rads radians to convert
 * @return degrees converted from radians
 */
static inline double to_degrees(double rads) {
    return rads * 180.0 / M_PI;
}

/**
 * @brief Constructs orbital element struct
 * @param J2000_day julian day
 * @param planet planet wanted
 */
orbital_elements::orbital_elements(double J2000_day, Planets planet) {
    switch (planet) {
    case SUN:
        N = 0.0;
        i = 0.0;
        w = 282.9404 + 4.70935E-5 * J2000_day;
        a = 1.0;
        e = 0.016709 - 1.151E-9 * J2000_day;
        M = 356.0470 + 0.9856002585 * J2000_day;
        break;
    case MOON:
        N = 125.1228 - 0.0529538083 * J2000_day;
        i = 5.1454;
        w = 318.0634 + 0.1643573223 * J2000_day;
        a = 60.2666;
        e = 0.054900;
        M = 115.3654 + 13.0649929509 * J2000_day;
        break;
    case MERCURY:
        N =  48.3313 + 3.24587E-5 * J2000_day;
        i = 7.0047 + 5.00E-8 * J2000_day;
        w =  29.1241 + 1.01444E-5 * J2000_day;
        a = 0.387098;
        e = 0.205635 + 5.59E-10 * J2000_day;
        M = 168.6562 + 4.0923344368 * J2000_day;
        break;
    case VENUS:
        N =  76.6799 + 2.46590E-5 * J2000_day;
        i = 3.3946 + 2.75E-8 * J2000_day;
        w =  54.8910 + 1.38374E-5 * J2000_day;
        a = 0.723330;
        e = 0.006773 - 1.302E-9 * J2000_day;
        M =  48.0052 + 1.6021302244 * J2000_day;
        break;
    case MARS:
        N =  49.5574 + 2.11081E-5 * J2000_day;
        i = 1.8497 - 1.78E-8 * J2000_day;
        w = 286.5016 + 2.92961E-5 * J2000_day;
        a = 1.523688;
        e = 0.093405 + 2.516E-9 * J2000_day;
        M =  18.6021 + 0.5240207766 * J2000_day;
        break;
    case JUPITER:
        N = 100.4542 + 2.76854E-5 * J2000_day;
        i = 1.3030 - 1.557E-7 * J2000_day;
        w = 273.8777 + 1.64505E-5 * J2000_day;
        a = 5.20256;
        e = 0.048498 + 4.469E-9 * J2000_day;
        M =  19.8950 + 0.0830853001 * J2000_day;
        break;
    case SATURN:
        N = 113.6634 + 2.38980E-5 * J2000_day;
        i = 2.4886 - 1.081E-7 * J2000_day;
        w = 339.3939 + 2.97661E-5 * J2000_day;
        a = 9.55475;
        e = 0.055546 - 9.499E-9 * J2000_day;
        M = 316.9670 + 0.0334442282 * J2000_day;
        break;
    case URANUS:
        N =  74.0005 + 1.3978E-5 * J2000_day;
        i = 0.7733 + 1.9E-8 * J2000_day;
        w =  96.6612 + 3.0565E-5 * J2000_day;
        a = 19.18171 - 1.55E-8 * J2000_day;
        e = 0.047318 + 7.45E-9 * J2000_day;
        M = 142.5905 + 0.011725806 * J2000_day;
        break;
    case NEPTUNE:
        N = 131.7806 + 3.0173E-5 * J2000_day;
        i = 1.7700 - 2.55E-7 * J2000_day;
        w = 272.8461 - 6.027E-6 * J2000_day;
        a = 30.05826 + 3.313E-8 * J2000_day;
        e = 0.008606 + 2.15E-9 * J2000_day;
        M = 260.2471 + 0.005995147 * J2000_day;
        break;
    default:
        break;
    }
    // turning angles into radians since all the trig functions expect radians. also normalizing them between 0 and 2pi (0 and 360 degrees)
    N = to_rads(normalize_degrees(N));
    i = to_rads(normalize_degrees(i));
    w = to_rads(normalize_degrees(w));
    // a is not an angle
    // e is not an angle
    M = to_rads(normalize_degrees(M));
    // might be worth it to precompute the constants to radians since floating point calculations are expensive
}

/**
 * @brief Constructs Celestial object
 * @param planet planet wanted
 */
Celestial::Celestial(Planets planet) : planet(planet), trace_hours(0) {}

/**
 * @brief Calculates observer centered azimuthal coordinates of celestial object at a given date and time
 * @param date the wanted date and time
 * @return the celestial objects azimuthal coordinates for the given date
 * @note observer coordinates needs to be set before calling this function
 */
azimuthal_coordinates Celestial::get_coordinates(const datetime_t &date) {
    double J2000 = datetime_to_j2000_day(date);
    orbital_elements oe(J2000, planet);
    orbital_elements sun(J2000, SUN);

    double E = eccentric_anomaly(oe.e, oe.M);
    rect_coordinates xy = to_rectangular_coordinates(oe.a, oe.e, E); // rectangular coordinates in lunar orbit
    double v = true_anomaly(xy);
    double r = distance(xy); // Earth radii in case of moon, Astronomical units otherwise

    rect_coordinates xyz = to_rectangular_coordinates(oe.N, oe.i, oe.w, v, r); // rectangular coordinates in ecliptic orbit
    // next up pertubations
    // Might need to do single calculations of needed orbital elements. right now theres couple of unnecessary calculations
    if ((planet == MOON) || (planet == SATURN) || (planet == JUPITER) || (planet == URANUS)) {
        ecliptic_coordinates ecl = to_ecliptic_coordinates(xyz);
        ecl.distance = r;
        ecliptic_coordinates perturbations(0,0,0);
        if (planet == MOON) {
            perturbations = perturbation_moon(oe, sun);
        } else if (planet == SATURN) {
            orbital_elements jupiter(J2000, JUPITER);
            perturbations = perturbation_saturn(jupiter.M, oe.M);
        } else if (planet == JUPITER) {
            orbital_elements saturn(J2000, SATURN);
            perturbations = perturbation_jupiter(oe.M, saturn.M);
        } else if (planet == URANUS) {
            orbital_elements jupiter(J2000, JUPITER);
            orbital_elements saturn(J2000, SATURN);
            perturbations = perturbation_uranus(oe.M, jupiter.M, saturn.M);
        }
        //ecl = ecl + perturbations; // this shit doesn't work
        ecl.lat = ecl.lat + perturbations.lat;
        ecl.lon = ecl.lon + perturbations.lon;
        ecl.distance = ecl.distance + perturbations.distance;
        xyz = to_rectangular_coordinates(ecl);
    }
    
    double obliquity = obliquity_of_eplectic(J2000);

    if (planet != MOON && planet != SUN) {
        double sun_E = eccentric_anomaly(sun.e, sun.M);
        rect_coordinates sun_xy = to_rectangular_coordinates(sun.a, sun.e, sun_E);
        double sun_v = true_anomaly(sun_xy);
        double sun_r = distance(sun_xy);
        double sun_lon = sun_v + sun.w;
        double sun_x = sun_r * cos(sun_lon);
        double sun_y = sun_r * sin(sun_lon);
        xyz.x = xyz.x + sun_x;
        xyz.y = xyz.y + sun_y;
    }
    xyz = rotate_through_obliquity_of_eplectic(xyz, obliquity);
    spherical_coordinates sc = to_spherical_coordinates(xyz);

    double lst = local_sidereal_time(J2000, observer_coordinates.longitude);
    double hour_angle = normalize_radians(lst - sc.RA); // TODO: normalize between -pi and pi

    // TODO: put this in a function or something
    double x = cos(hour_angle) * cos(sc.DECL);
    double y = sin(hour_angle) * cos(sc.DECL);
    double z = sin(sc.DECL);

    double x_horizontal = x * sin(to_rads(observer_coordinates.latitude)) - z * cos(to_rads(observer_coordinates.latitude));
    double z_horizontal = x * cos(to_rads(observer_coordinates.latitude)) + z * sin(to_rads(observer_coordinates.latitude));
    azimuthal_coordinates ac;
    ac.azimuth = atan2(y, x_horizontal) + M_PI;
    ac.altitude = atan2(z_horizontal, sqrt(x_horizontal*x_horizontal + y*y));

    // next up parallax
    double parallax = 0;
    if (planet == MOON) {
        parallax = asin(1 / r);
    } else {
        parallax = 4.26345151167726e-05 / r;
    }
    ac.altitude = ac.altitude - parallax * cos(ac.altitude);
    return ac;
}

/**
 * @brief Prints coordinates every hour after a given date and time
 * @param start_date the date at which to start printing
 * @param hours the number of hours to print
 */
void Celestial::print_coordinates(datetime_t start_date, int hours) {
    std::cout << (int)start_date.year << ", " << (int)start_date.month << ", " << (int)start_date.day << ", " << (int)start_date.hour << ", " << (int)start_date.min << std::endl;

    for (int i=0; i<hours; i++) {
        azimuthal_coordinates coord = get_coordinates(start_date);
        std::cout << coord.altitude << ", " << coord.azimuth << std::endl;
        datetime_increment_hour(start_date);
    }
    std::cout << "end" << std::endl;
}

/**
 * @brief Gets a Command of celestial object in zenith, rise or fall.
 * @param point the interest point to get
 * @param start_date the date from which to start searching
 * @return Command with time and coordinates 
 * @note Command id needs to be set after this function
 */
Command Celestial::get_interest_point_command(Interest_point point, const datetime_t &start_date) {
    if (point == NOW) {
        Command cmd = {0};
        cmd.coords = get_coordinates(start_date);
        return cmd;
    }
    std::vector<Command> interesting_commands = get_interesting_commands(start_date);
    if (point == ZENITH) return interesting_commands[1];
    if (point == ABOVE) return interesting_commands[0];
    if (point == BELOW) return interesting_commands[2];


    return interesting_commands[1];
}

/**
 * @brief Checks if the coordinates are above the horizon
 * @param current the coordinates to check
 * @return true if the coordinates are above horizon
 * @return false if the coordinates are below horizon
 */
bool Celestial::check_for_above_horizon(const azimuthal_coordinates &current) {
    return current.altitude > 0;
}
/**
 * @brief Checks if the coordinates are rising
 * @param current the coordinates to check
 * @param next the next coordinates to compare to
 * @return true if the coordinates are rising
 * @return false if the coordinates are not rising
 */
bool Celestial::check_for_rising(const azimuthal_coordinates &current, const azimuthal_coordinates &next) {
    if (current.altitude > 0) {
        if (current.altitude - next.altitude < 0) {
            return true;
        }
    }
    return false;
}
/**
 * @brief Checks if the coordinates are falling
 * @param current the coordinates to check
 * @param next the next coordinates to compare to
 * @return true if the coordinates are falling
 * @return false if the coordinates are not falling
 */
bool Celestial::check_for_falling(const azimuthal_coordinates &current, const azimuthal_coordinates &next) {
    if (current.altitude > 0) {
        if (next.altitude < 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks if the coordinates are the zenith
 * @param last the last coordinates to compare to
 * @param current the coordinates to check
 * @param next the next coordinates to compare to
 * @return true if the coordinates are zenith
 * @return false if the coordinates are not zenith
 */
bool Celestial::check_for_zenith(const azimuthal_coordinates & last, const azimuthal_coordinates &current, const azimuthal_coordinates &next) {
    if ((last.altitude < current.altitude) && (next.altitude < current.altitude)) {
        return true;
    }
    return false;
}
/**
 * @brief Calculates interesting commands
 * @param start_date the date and time to start the search from
 * @return Vector of commands with the interesting commands
 */
std::vector<Command> Celestial::get_interesting_commands(const datetime_t &start_date) {
    std::vector<Command> result = {{0}, {0}, {0}};
    for (auto &res : result) {
        res.time.year = -1;
    }
    datetime_t last_date = start_date;
    datetime_t current_date = start_date;
    azimuthal_coordinates last = get_coordinates(last_date);
    // datetime_increment_hour(current_date);
    for (int x=0; x < 10; x++) {
        datetime_increment_minute(current_date);
    }
    azimuthal_coordinates current = get_coordinates(current_date);
    datetime_t next_date = current_date;
    // datetime_increment_hour(next_date);
    for (int x=0; x < 10; x++) {
        datetime_increment_minute(next_date);
    }
    azimuthal_coordinates next = get_coordinates(next_date);
    bool done = false;
    bool rising_found = false;
    bool zenith_found = false;
    bool falling_found = false;
    int i = 0;
    while (!done) {
        if (check_for_above_horizon(current)) {
            if (check_for_rising(current, next) && !rising_found) {
                rising_found = true;
                result[0] = {1, current, current_date};
            }
            else if (check_for_zenith(last, current, next) && !zenith_found) {
                datetime_t iter_date_last = last_date;
                azimuthal_coordinates iter_coords_last = last;
                datetime_t iter_date_current = last_date;
                datetime_increment_minute(iter_date_current);
                azimuthal_coordinates iter_coords_current = get_coordinates(iter_date_current);
                datetime_t iter_date_next = iter_date_current;
                datetime_increment_minute(iter_date_next);
                azimuthal_coordinates iter_coords_next = get_coordinates(iter_date_next);
                int j = 0;
                while (!check_for_zenith(iter_coords_last, iter_coords_current, iter_coords_next) && j < 120) {
                    datetime_increment_minute(iter_date_last);
                    datetime_increment_minute(iter_date_current);
                    datetime_increment_minute(iter_date_next);
                    iter_coords_last = iter_coords_current;
                    iter_coords_current = iter_coords_next;
                    iter_coords_next = get_coordinates(iter_date_next);
                    j++;
                }
                zenith_found = true;
                result[1] = {1, iter_coords_current, iter_date_current};
            }
            else if (check_for_falling(current, next) && !falling_found) {
                falling_found = true;
                result[2] = {1, current, current_date};
            }
        }
        // datetime_increment_hour(last_date);
        for (int x=0; x < 10; x++) {
            datetime_increment_minute(last_date);
        }
        // datetime_increment_hour(current_date);
        for (int x=0; x < 10; x++) {
            datetime_increment_minute(current_date);
        }
        // datetime_increment_hour(next_date);
        for (int x=0; x < 10; x++) {
            datetime_increment_minute(next_date);
        }
        last = current;
        current = next;
        next = get_coordinates(next_date);
        i++;
        if (result[0].id != 0 && result[1].id != 0 && result[2].id != 0) done = true;
        if (i > 48 * 6) {
            done = true;
        }
    }

    return result;
}

/**
 * @brief Sets the observer coordinates
 * @param observer_coordinates the coordinates to use
 */
void Celestial::set_observer_coordinates(const Coordinates observer_coordinates) {
    this->observer_coordinates = observer_coordinates;
}

/**
 * @brief Starts a trace
 * @param start_datetime the time that the trace starts at
 * @param hours the amount of hours to trace
 */
void Celestial::start_trace(datetime_t start_datetime, int hours) {
    trace_date = start_datetime;
    trace_hours = hours;
}

/**
 * @brief Calculates the next coordinates for a trace
 * @return Command of the trace trace
 * @note trace needs to be started before calling this function
 */
Command Celestial::next_trace(void) {
    Command result;
    if (trace_hours <= 0) {
        result.time.year = -1; // this indicates error maybe
        return result;
    }
    result.coords = get_coordinates(trace_date);
    result.time = trace_date;
    datetime_increment_hour(trace_date);
    trace_hours--;
    return result;
}

/**
 * @brief Gets the planet the celestial object is referring to
 * @return The integer representation of the planet
 */
int Celestial::get_planet(void) {
    return (int)planet;
}

/**
 * @brief Prints the planet of the celestial object to stdout
 */
void Celestial::print_planet(void) {
    switch (planet)
    {
    case SUN:
        DEBUG("SUN");
        break;
    case MOON:
        DEBUG("MOON");
        break;
    case MERCURY:
        DEBUG("MERCURY");
        break;
    case VENUS:
        DEBUG("VENUS");
        break;
    case MARS:
        DEBUG("MARS");
        break;
    case JUPITER:
        DEBUG("JUPITER");
        break;
    case SATURN:
        DEBUG("SATURN");
        break;
    case URANUS:
        DEBUG("URANUS");
        break;
    case NEPTUNE:
        DEBUG("NEPTUNE");
        break;
    
    default:
        break;
    }
}

/**
 * @brief Calculates eccentric anomaly
 * @param e orbital element
 * @param M orbital element
 * @return eccentric anomaly
 */
double eccentric_anomaly(double e, double M) {
    double E0 = M + e * sin(M) * (1.0 + e * cos(M));
    double E1 = 0;
    int iterations = 0;
    while ((fabs(E0 - E1) > ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR) && (iterations < ECCENTRIC_ANOMALY_APPROXXIMATION_MAX_ITER)) {
        E1 = E0;
        E0 = E0 - (E0 - e * sin(E0) - M) / (1 - e * cos(E0));
        iterations++;
    }
    if (fabs(E0 - E1) > ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR)
        DEBUG("Eccentric anomaly calculation didn't meet the approximation error: ",
                ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR,
                " in ", ECCENTRIC_ANOMALY_APPROXXIMATION_MAX_ITER, "iterations");
    return E0; // in radians
}

/**
 * @brief Calculates true anomaly
 * @param coords rectangular coordinates
 * @return true anomaly
 */
double true_anomaly(rect_coordinates coords) {
    return normalize_radians(atan2(coords.y, coords.x));
}

/**
 * @brief Calculates distance of celestial object
 * @param coords rectangular coordinates
 * @return distance
 */
double distance(rect_coordinates coords) {
    return sqrt(coords.x*coords.x + coords.y*coords.y);
}

/**
 * @brief Calculates perturbation of moons orbit
 * @param moon orbital elements of moon
 * @param sun orbital elements of sun
 * @return perturbations in coordinates
 */
ecliptic_coordinates perturbation_moon(const orbital_elements &moon, const orbital_elements &sun) {
    double L_sun = normalize_radians(sun.M + sun.w); // mean longitude of the sun
    double L_moon = moon.M + moon.w + moon.N; // mean longitude of the moon
    double D = L_moon - L_sun; // mean elongation of the moon
    double F = L_moon - moon.N; // argument of latitude for the moon
    ecliptic_coordinates result(0,0,0);
    // longitude corrections
    result.lon += to_rads(-1.274) * sin(moon.M - 2*D);   // Evection
    result.lon += to_rads(+0.658) * sin(2*D);            // variation
    result.lon += to_rads(-0.186) * sin(sun.M);          // Yearly equation
    result.lon += to_rads(-0.059) * sin(2*moon.M - 2*D);
    result.lon += to_rads(-0.057) * sin(moon.M - 2*D + sun.M);
    result.lon += to_rads(+0.053) * sin(moon.M + 2*D);
    result.lon += to_rads(+0.046) * sin(2*D - sun.M);
    result.lon += to_rads(+0.041) * sin(moon.M - sun.M);
    result.lon += to_rads(-0.035) * sin(D);              // Parallatic equation
    result.lon += to_rads(-0.031) * sin(moon.M + sun.M);
    result.lon += to_rads(-0.015) * sin(2*F - 2*D);
    result.lon += to_rads(+0.011) * sin(moon.M - 4*D);
    // latitude corrections
    result.lat += to_rads(-0.173) * sin(F - 2*D);
    result.lat += to_rads(-0.055) * sin(moon.M - F - 2*D);
    result.lat += to_rads(-0.046) * sin(moon.M + F - 2*D);
    result.lat += to_rads(+0.033) * sin(F + 2*D);
    result.lat += to_rads(+0.017) * sin(2*moon.M + F);
    // distance corrections
    result.distance += -0.58 * cos(moon.M - 2*D);
    result.distance += -0.46 * cos(2*D);
    return result;
}

/**
 * @brief Calculates perturbation of jupiters orbit
 * @param Mj orbital element of jupiter
 * @param Ms orbital element of saturn
 * @return perturbations in coordinates
 */
ecliptic_coordinates perturbation_jupiter(double Mj, double Ms) {
    ecliptic_coordinates result(0,0,0);
    result.lon += to_rads(-0.332) * sin(2*Mj - 5*Ms - 1.1798425);
    result.lon += to_rads(-0.056) * sin(2*Mj - 2*Ms + 0.3665191);
    result.lon += to_rads(+0.042) * sin(3*Mj - 5*Ms + 0.3665191);
    result.lon += to_rads(-0.036) * sin(Mj - 2*Ms);
    result.lon += to_rads(+0.022) * cos(Mj - Ms);
    result.lon += to_rads(+0.023) * sin(2*Mj - 3*Ms + 0.907571211);
    result.lon += to_rads(-0.016) * sin(Mj - 5*Ms - 1.204277183);
    return result;
}

/**
 * @brief Calculates perturbation of saturns orbit
 * @param Mj orbital element of jupiter
 * @param Ms orbital element of jupiter
 * @return perturbations in coordinates
 */
ecliptic_coordinates perturbation_saturn(double Mj, double Ms) {
    ecliptic_coordinates result(0,0,0);
    // longitude
    result.lon += to_rads(+0.812) * sin(2*Mj - 5*Ms - 1.179842574);
    result.lon += to_rads(-0.229) * cos(2*Mj - 4*Ms - 0.034906585);
    result.lon += to_rads(+0.119) * sin(Mj - 2*Ms - 0.052359877);
    result.lon += to_rads(+0.046) * sin(2*Mj - 6*Ms - 1.204277183);
    result.lon += to_rads(+0.014) * sin(Mj - 3*Ms + 0.55850536);
    // latitude
    result.lat += to_rads(-0.020) * cos(2*Mj - 4*Ms - 0.034906585);
    result.lat += to_rads(+0.018) * sin(2*Mj - 6*Ms - 0.85521133);
    return result;
}

/**
 * @brief Calculates perturbation of uranus orbit
 * @param Mu orbital element of uranus
 * @param Mj orbital element of jupiter
 * @param Ms orbital element of jupiter
 * @return perturbations in coordinates
 */
ecliptic_coordinates perturbation_uranus(double Mu, double Mj, double Ms) {
    ecliptic_coordinates result(0,0,0);
    result.lon += to_rads(+0.040) * sin(Ms - 2*Mu + 0.104719755);
    result.lon += to_rads(+0.035) * sin(Ms - 3*Mu + 0.57595865);
    result.lon += to_rads(-0.015) * sin(Mj - Mu + 0.34906585);
    return result;
}

/**
 * @brief Normalizes degrees to between 0 and 360
 * @param degrees degrees to normalize
 * @return normalized degrees
 */
double normalize_degrees(double degrees) {
    degrees = fmod(degrees, 360.0);
    if (degrees < 0) {
        degrees += 360.0;
    }
    return degrees;
}

/**
 * @brief Normalizes radians to between 0 and 2pi
 * @param radians radians to normalize
 * @return normalized radians
 */
double normalize_radians(double radians) {
    radians = fmod(radians, 2 * M_PI);
    if (radians < 0) {
        radians += 2 * M_PI;
    }
    return radians;
}

/**
 * @brief Converts datetime to julian day
 * @param date datetime_t object to convert
 * @return julian day
 */
double datetime_to_j2000_day(const datetime_t &date) {
    int d = 367*date.year - 7 * ( date.year + (date.month+9)/12 ) / 4 - 3 * ( ( date.year + (date.month-9)/7 ) / 100 + 1 ) / 4 + 275*date.month/9 + date.day - 730515;
    double ut = (double)date.hour + ((double)date.min / 60.0); // can add seconds too but there's no point
    return (double)d + ut / 24.0;
}

/**
 * @brief Calculates the local sidereal time
 * @param J2000_day julian day
 * @param longitude observers longitude
 * @return local sidereal time
 */
double local_sidereal_time(double J2000_day, double longitude) {
    double J = J2000_day + 2451543.5 - 2451545.0;
    double T = J / 36525.0;
    double LMST = 280.46061837 + 360.98564736629 * J + 0.000387933*T*T - T*T*T / 38710000.0 + longitude; // degrees
    return to_rads(normalize_degrees(LMST));

}

/**
 * @brief Calculates the obliquity of eplectic
 * @param J2000_day julian day
 * @return obliquity of eplectic
 */
double obliquity_of_eplectic(double J2000_day) {
    return to_rads(23.4393 - 3.563E-7 * J2000_day);
}

/**
 * @brief rotates rectangular coordinates through obliquity of eplectic
 * @param rc rectangular coordinates to rotate
 * @param obliquity obliquity of eplectic
 * @return rotated rectangular coordinates
 */
rect_coordinates rotate_through_obliquity_of_eplectic(const rect_coordinates &rc, double obliquity) {
    rect_coordinates result;
    result.x = rc.x;
    result.y = rc.y * cos(obliquity) - rc.z * sin(obliquity);
    result.z = rc.y * sin(obliquity) + rc.z * cos(obliquity);
    return result;
}

/**
 * @brief Converts spherical coordinates to rectangular coordinates
 * @param sp spherical coordinates to convert
 * @return rectangular coordinates
 */
rect_coordinates to_rectangular_coordinates(spherical_coordinates sp) {
    rect_coordinates result;
    result.x = sp.distance * cos(sp.RA) * cos(sp.DECL);
    result.y = sp.distance * sin(sp.RA) * cos(sp.DECL);
    result.z = sp.distance * sin(sp.DECL);
    return result;
}

/**
 * @brief Converts ecliptic coordinates to rectangular coordinates
 * @param ec ecliptic coordinates to convert
 * @return rectangular coordinates
 */
rect_coordinates to_rectangular_coordinates(ecliptic_coordinates ec) {
    // same as from spherical coordinates. new function for clarity
    rect_coordinates result;
    result.x = ec.distance * cos(ec.lon) * cos(ec.lat);
    result.y = ec.distance * sin(ec.lon) * cos(ec.lat);
    result.z = ec.distance * sin(ec.lat);
    return result;
}

/**
 * @brief Calculates rectangular coordinates from orbital elements and eccentic anomaly
 * @param a orbital element
 * @param e orbital element
 * @param E eccentic anomaly
 * @return rectangular coordinates
 */
rect_coordinates to_rectangular_coordinates(double a, double e, double E) {
    rect_coordinates result;
    result.x = a * (cos(E) - e);
    result.y = a * (sqrt(1.0 - e*e) * sin(E));
    result.z = 1.0;
    return result;
}

/**
 * @brief Calculates rectangular coordinates from orbital elements, distance and true anomaly
 * @param N orbital element
 * @param i orbital element
 * @param w orbital element
 * @param v true anomaly
 * @param r distance
 * @return rectangular coordinates
 */
rect_coordinates to_rectangular_coordinates(double N, double i, double w, double v, double r) {
    rect_coordinates result;
    result.x = r * (cos(N) * cos(v+w) - sin(N) * sin(v+w) * cos(i));
    result.y = r * (sin(N) * cos(v+w) + cos(N) * sin(v+w) * cos(i));
    result.z = r * sin(v+w) * sin(i);
    return result;
}

/**
 * @brief Converts rectangular coordinates to spherical coordinates
 * @param rc rectangular coordinates to convert
 * @return spherical coordinates
 */
spherical_coordinates to_spherical_coordinates(rect_coordinates rc) {
    spherical_coordinates result;
    result.RA = normalize_radians(atan2(rc.y, rc.x));
    result.DECL = atan2(rc.z, sqrt(rc.x*rc.x + rc.y*rc.y));
    result.distance = sqrt(rc.x*rc.x + rc.y*rc.y + rc.z*rc.z);
    if (result.distance > 0.999 && result.distance < 1.001) result.distance = 1.0;
    return result;
}

/**
 * @brief Converts rectangular coordinates to ecliptic coordinates
 * @param rc rectangular coordinates to convert
 * @return ecliptic coordinates
 */
ecliptic_coordinates to_ecliptic_coordinates(rect_coordinates rc) {
    // this is same as to spherical coordinates but without distance. Done so we don't mix these
    ecliptic_coordinates result;
    result.lon = normalize_radians(atan2(rc.y, rc.x));
    result.lat = atan2(rc.z, sqrt(rc.x*rc.x + rc.y*rc.y));
    result.distance = 1; // this gets set elsewhere. Doing this so we don't need to redundant expensive calculations.
    return result;
}