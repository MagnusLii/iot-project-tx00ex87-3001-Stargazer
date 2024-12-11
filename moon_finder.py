# Example calculations to find angle of the moon from observers perspective
# NEEDED:
# date and time
# Observers latitude and longitude

import datetime
import math

def calculate_julian_day(date: datetime):
    '''
    Julian date is the number of days since Epoch (12:00 January 1, 4713 BC)
    https://quasar.as.utexas.edu/BillInfo/JulianDatesG.html
    http://www.geoastro.de/elevazmoon/basics/meeus.htm#jd
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


def calculate_julian_centuries(julian_date: float):
    return (julian_date - 2451545.0) / 36525


def calculate_obliquity_of_ecliptic(julian_centuries: float):
    return 23.0 + 26.0/60.0 + 21.448/3600.0 - (46.8150*julian_centuries+ 0.00059*math.pow(julian_centuries, 2)- 0.001813*math.pow(julian_centuries, 3))/3600


if __name__ == "__main__":
    date = datetime.datetime.now()
    date2 = datetime.datetime(1991, 5, 19, 13)
    JD = calculate_julian_day(date2)
    print(JD)
    JC = calculate_julian_centuries(JD)
    print (JC)
    oblique = calculate_obliquity_of_ecliptic(JC)
    print(oblique)