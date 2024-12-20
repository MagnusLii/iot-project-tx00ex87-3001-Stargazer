#include "planet_finder.hpp"

#define ECCENTRIC_ANOMALY_APPROXXIMATION_MAX_ITER 3
#define ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR 0.001

static inline double to_rads(double degrees) {
    return degrees * M_PI / 180.0;
}

static inline double to_degrees(double rads) {
    return rads * 180.0 / M_PI;
}


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

double Celestial::eccentric_anomaly(double e, double M) {
    double E0 = M + e * sin(M) * (1.0 + e * cos(M));
    double E1 = 0;
    int iterations = 0;
    while ((fabs(E0 - E1) > ECCENTRIC_ANOMALY_APPROXXIMATION_ERROR) && (iterations < ECCENTRIC_ANOMALY_APPROXXIMATION_MAX_ITER)) {
        E1 = E0;
        E0 = E0 - (E0 - e * sin(E0) - M) / (1 - e * cos(E0));
        iterations++;
    }
    return E0; // in radians
}

double Celestial::true_anomaly(double a, double e, double E) {
    double xv = a * (cos(E) - e);
    double yv = a * (sqrt(1.0 - e*e) * sin(E));
    return atan2(yv, xv);
}

double Celestial::distance(double e, double E) {
    double xv = a * (cos(E) - e);
    double yv = a * (sqrt(1.0 - e*e) * sin(E));
    return sqrt(xv*xv + yv*yv);
}

double normalize_degrees(double degrees) {
    degrees = fmod(degrees, 360.0);
    if (degrees < 0) {
        degrees += 360.0;
    }
    return degrees;
}

double normalize_radians(double radians) {
    radians = fmod(radians, 2 * M_PI);
    if (radians < 0) {
        radians += 2 * M_PI;
    }
    return radians;
}


double datetime_to_j2000_day(datetime_t &date) {
    int d = 367*date.year - 7 * ( date.year + (date.month+9)/12 ) / 4 - 3 * ( ( date.year + (date.month-9)/7 ) / 100 + 1 ) / 4 + 275*date.month/9 + date.day - 730515;
    double ut = (double)date.hour + ((double)date.min / 60.0); // can add seconds too but there's no point
    return (double)d + ut / 24.0;
}

double local_sidereal_time(double J2000_day, double longitude) {
    double J = J2000_day + 2451543.5 - 2451545.0;
    double T = J / 36525.0;
    double LMST = 280.46061837 + 360.98564736629 * J + 0.000387933*T*T - T*T*T / 38710000.0 + longitude; // degrees
    return normalize_degrees(LMST);

}

double obliquity_of_eplectic(double J2000_day) {
    return 23.4393 - 3.563E-7 * J2000_day;
}

rect_coordinates to_rectangular_coordinates(spherical_coordinates sp) {
    rect_coordinates result;
    result.x = sp.distance * cos(sp.RA) * cos(sp.DECL);
    result.y = sp.distance * sin(sp.RA) * cos(sp.DECL);
    result.z = sp.distance * sin(sp.DECL);
    return result;
}

spherical_coordinates to_spherical_coordinates(rect_coordinates rc) {
    spherical_coordinates result;
    result.RA = atan2(rc.y, rc.x);
    result.DECL = atan2(rc.z, sqrt(rc.x*rc.x + rc.y*rc.y));
    result.distance = sqrt(rc.x*rc.x + rc.y*rc.y + rc.z*rc.z);
    if (result.distance > 0.999 && result.distance < 1.001) result.distance = 1.0;
    return result;
}