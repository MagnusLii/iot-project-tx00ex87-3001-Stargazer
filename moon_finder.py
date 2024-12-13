# Example calculations to find angle of the moon from observers perspective
# NEEDED:
# date and time
# Observers latitude and longitude

import datetime
import math

def normalize_angle_degrees(angle_degrees: float):
    while angle_degrees < 0:
        angle_degrees += 360
    while angle_degrees > 360:
        angle_degrees -=360
    return angle_degrees

def to_radians(angle_degrees: float):
    return angle_degrees * math.pi / 180

def to_degrees(angle_radians: float):
    return angle_radians * 180 / math.pi


def calculate_julian_day(date: datetime):
    '''
    Julian date is the number of days since Epoch (12:00 January 1, 4713 BC)
    https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html
    http://www.geoastro.de/elevazmoon/basics/meeus.htm#jd
    http://stjarnhimlen.se/comp/tutorial.html#4
    Note that the formula above is valid only from March 1900 to February 2100
    '''
    year = date.year
    month = date.month
    day = date.day
    hour = date.hour + (date.minute / 60)
    if (month <= 2):
        month += 12
        year -= 1
    year_calculation = math.floor(365.25 * year)
    month_calculation = math.floor(30.6001 * (month + 1))
    hour_calculation = hour / 24
    return year_calculation + month_calculation - 15 + 1720996.5 + day + hour_calculation


def calculate_J2000_day(julian_date: float):
    return julian_date - 2451543.5

def calculate_julian_centuries(julian_date: float):
    return (julian_date - 2451545.0) / 36525

def calculate_local_sidereal_time(J2000_date: float, longitude: float):
    '''longitude: eastern longitudes positive, western negative'''
    julian_date = J2000_date + 2451543.5
    T = calculate_julian_centuries(julian_date)
    theta0 = 280.46061837 + 360.98564736629*(julian_date-2451545.0) + 0.000387933*T*T - T*T*T/38710000
    theta0 = normalize_angle_degrees(theta0)
    return normalize_angle_degrees(theta0 + longitude)


def calculate_moon_hour_angle(local_sidereal_time: float, moon_RA: float):
    return local_sidereal_time - moon_RA

def calculate_moon_orbital_elements(J2000_date: float):
    '''
    http://stjarnhimlen.se/comp/tutorial.html#7
    The Moon's ascending node is moving in a retrogade ("backwards") direction one revolution in about 18.6 years.
    The Moon's perigee (the point of the orbit closest to the Earth) moves in a "forwards" direction one revolution in about 8.8 years.
    The Moon itself moves one revolution in about 27.5 days. The mean distance, or semi-major axis, is expressed in Earth equatorial radii).
    '''
    N = normalize_angle_degrees(125.1228 - 0.0529538083 * J2000_date)    # Long asc. node
    i = normalize_angle_degrees(5.1454)                                  # Inclination
    w = normalize_angle_degrees(318.0634 + 0.1643573223  * J2000_date)   # Arg. of perigee
    a = 60.2666                                                          # Mean distance
    e = 0.054900                                                         # Eccentricity
    M = normalize_angle_degrees(115.3654 + 13.0649929509 * J2000_date)   # Mean anomaly
    return (N, i, w, a, e, M) # in degrees

def calclulate_moon_eccentric_anomaly(eccentricity_e: float, mean_anomaly_M: float, max_error: float = 0.5):
    e = eccentricity_e
    M = mean_anomaly_M
    E0 = M + (180/math.pi) * e * math.sin(to_radians(M)) * (1 + e * math.cos(to_radians(M)))
    E1 = 0
    while abs(E0 - E1) > max_error:
        E1 = E0
        E0 = E0 - (E0 - (180/math.pi) * e * math.sin(to_radians(E0)) - M) / (1 - e * math.cos(to_radians(E0)))
    return E0 # in degrees


def calculate_moon_rectangular_coordinates(mean_distance_a: float, eccentricity_e: float, eccentric_anomaly_E: float):
    a = mean_distance_a
    e = eccentricity_e
    E = eccentric_anomaly_E
    x = a * (math.cos(to_radians(E)) - e)
    y = a * math.sqrt(1 - e*e) * math.sin(to_radians(E))
    return (x, y)


def calculate_moon_distance(rect_coord_x: float, rect_coord_y: float):
    x = rect_coord_x
    y = rect_coord_y
    return math.sqrt(x*x + y*y) # distance in earth radii


def calculate_moon_true_anomaly(rect_coord_x: float, rect_coord_y: float):
    x = rect_coord_x
    y = rect_coord_y
    return to_degrees(math.atan2(y, x)) # in degrees

def calculate_moon_eplectic_long_lat_dist_inaccurate(J2000_date):
    N, i, w, a, e, M = calculate_moon_orbital_elements(J2000_date)
    E = calclulate_moon_eccentric_anomaly(e, M)
    x, y = calculate_moon_rectangular_coordinates(a, e, E)
    r = calculate_moon_distance(x, y)
    v = calculate_moon_true_anomaly(x, y)

    xeclip = r * ( math.cos(to_radians(N)) * math.cos(to_radians(v + w)) - math.sin(to_radians(N)) * math.sin(to_radians(v + w)) * math.cos(to_radians(i)) )
    yeclip = r * ( math.sin(to_radians(N)) * math.cos(to_radians(v + w)) + math.cos(to_radians(N)) * math.sin(to_radians(v + w)) * math.cos(to_radians(i)) )
    zeclip = r * math.sin(to_radians(v + w)) * math.sin(to_radians(i))
    
    long = to_degrees(math.atan2(yeclip, xeclip))
    lat = to_degrees(math.atan2( zeclip, math.sqrt(xeclip*xeclip + yeclip*yeclip) ))
    r = math.sqrt( xeclip*xeclip + yeclip*yeclip + zeclip*zeclip )
    return (normalize_angle_degrees(long), normalize_angle_degrees(lat), r)
    

def calculate_moon_eplectic_long_lat(J2000_date: float):
    inaccurate_long, inaccurate_lat, dist = calculate_moon_eplectic_long_lat_dist_inaccurate(J2000_date)
    pertubation_long = calculate_moon_pertubations_long(J2000_date)
    pertubation_lat = calculate_moon_pertubations_latitude(J2000_date)
    return (inaccurate_long + pertubation_long, inaccurate_lat + pertubation_lat)

def calculate_moon_RA_DECL(J2000_date: float):
    long, lat = calculate_moon_eplectic_long_lat(J2000_date)
    oblecl = calculate_obliquity_of_ecliptic(J2000_date)

    xeclip = math.cos(to_radians(long)) * math.cos(to_radians(lat))
    yeclip = math.sin(to_radians(long)) * math.cos(to_radians(lat))
    zeclip = math.sin(to_radians(lat))

    xequat = xeclip
    yequat = yeclip * math.cos(to_radians(oblecl)) - zeclip * math.sin(to_radians(oblecl))
    zequat = yeclip * math.sin(to_radians(oblecl)) + zeclip * math.cos(to_radians(oblecl))

    RA = to_degrees(math.atan2(yequat, xequat))
    DECL = to_degrees(math.atan2(zequat, math.sqrt(xequat*xequat + yequat*yequat)))

    return (normalize_angle_degrees(RA), DECL)


def calculate_moon_alt_az_inaccurate(J2000_date: float, long: float, lat: float):
    RA, DECL = calculate_moon_RA_DECL(J2000_date)
    lct = calculate_local_sidereal_time(J2000_date, long)
    ha = calculate_moon_hour_angle(lct, RA)

    x = math.cos(to_radians(ha)) * math.cos(to_radians(DECL))
    y = math.sin(to_radians(ha)) * math.cos(to_radians(DECL))
    z = math.sin(to_radians(DECL))

    xhor = x * math.sin(to_radians(lat)) - z * math.cos(to_radians(lat))
    yhor = y
    zhor = x * math.cos(to_radians(lat)) + z * math.sin(to_radians(lat))

    azimuth = to_degrees(math.atan2(yhor, xhor)) + 180
    altitude = to_degrees(math.asin(zhor))
    return (altitude, azimuth)


def calculate_moon_parallax(J2000_date: float):
    N, i, w, a, e, M = calculate_moon_orbital_elements(J2000_date)
    E = calclulate_moon_eccentric_anomaly(e, M)
    x, y = calculate_moon_rectangular_coordinates(a, e, E)
    r = calculate_moon_distance(x, y)
    return to_degrees(math.asin(1 / r))

def calculate_moon_alt_az(J2000_date: float, long: float, lat: float):
    alt_inaccurate, az = calculate_moon_alt_az_inaccurate(J2000_date, long, lat)
    parallax = calculate_moon_parallax(J2000_date)
    return (alt_inaccurate - parallax, az)



def calculate_sun_mean_anomaly(J2000_date: float):
    return 356.0470 + 0.9856002585 * J2000_date # in degrees

def calculate_sun_mean_longitude(J2000_date: float, sun_mean_anomaly: float):
    w = 282.9404 + 4.70935E-5 * J2000_date # longitude of perihelion
    return w + sun_mean_anomaly

def calculate_moon_pertubation_arguments(J2000_date:float):
    N, i, w, a, e, M = calculate_moon_orbital_elements(J2000_date)
    Ms = normalize_angle_degrees(calculate_sun_mean_anomaly(J2000_date))                 # Sun's  mean anomaly
    Ls = normalize_angle_degrees(calculate_sun_mean_longitude(J2000_date, Ms))           # Sun's  mean longitude
    Lm = normalize_angle_degrees(N + w + M)                                              # Moon's mean longitude
    Mm = normalize_angle_degrees(M)                                                      # Moon's mean anomaly
    D = normalize_angle_degrees(Lm - Ls)                                                 # Moon's mean elongation
    F = normalize_angle_degrees(Lm - N)                                                  # Moon's argument of latitude
    return (Ls, Lm, Ms, Mm, D, F)


def calculate_moon_pertubations_long(J2000_date: float):
    Ls, Lm, Ms, Mm, D, F = calculate_moon_pertubation_arguments(J2000_date)
    pertubation = 0
    pertubation += -1.274 * math.sin(to_radians(Mm - 2*D))      # Evection
    pertubation += +0.658 * math.sin(to_radians(2*D))           # Variation
    pertubation += -0.186 * math.sin(to_radians(Ms))            # Yearly equation
    pertubation += -0.059 * math.sin(to_radians(2*Mm - 2*D))
    pertubation += -0.057 * math.sin(to_radians(Mm - 2*D + Ms))
    pertubation += +0.053 * math.sin(to_radians(Mm + 2*D))
    pertubation += +0.046 * math.sin(to_radians(2*D - Ms))
    pertubation += +0.041 * math.sin(to_radians(Mm - Ms))
    pertubation += -0.035 * math.sin(to_radians(D))             # Parallactic equation
    pertubation += -0.031 * math.sin(to_radians(Mm + Ms))
    pertubation += -0.015 * math.sin(to_radians(2*F - 2*D))
    pertubation += +0.011 * math.sin(to_radians(Mm - 4*D))
    return pertubation

def calculate_moon_pertubations_latitude(J2000_date: float):
    Ls, Lm, Ms, Mm, D, F = calculate_moon_pertubation_arguments(J2000_date)
    pertubation = 0
    pertubation += -0.173 * math.sin(to_radians(F - 2*D))
    pertubation += -0.055 * math.sin(to_radians(Mm - F - 2*D))
    pertubation += -0.046 * math.sin(to_radians(Mm + F - 2*D))
    pertubation += +0.033 * math.sin(to_radians(F + 2*D))
    pertubation += +0.017 * math.sin(to_radians(2*Mm + F))
    return pertubation

def calculate_obliquity_of_ecliptic(J2000_date: float):
    return 23.4393 - 3.563E-7 * J2000_date

def calculate_moon_position(date: datetime, longitude: float, latitude: float):
    JD = calculate_julian_day(date)
    J2000 = calculate_J2000_day(JD)
    return calculate_moon_alt_az(J2000, longitude, latitude)


if __name__ == "__main__":
    date = datetime.datetime.now(datetime.timezone.utc)
    date2 = datetime.datetime(1991, 5, 19, 13)
    alt, az = calculate_moon_position(date, 24.931695, 60.180726)
    print("Altitude:", alt, "Azimuth:", az)