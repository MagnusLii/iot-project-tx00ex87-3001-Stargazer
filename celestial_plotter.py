import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import math
import serial
import datetime



port = serial.Serial('/dev/ttyACM0', 115200)
done = False

altitudes = []
azimuths = []
print("reading...")
start_date_str = port.readline().decode("utf-8").strip().replace('\0', '')
stop_date_str = port.readline().decode("utf-8").strip().replace('\0', '')
start_date = datetime.datetime.strptime(start_date_str, "%Y, %m, %d, %H, %M")
dates = [start_date + datetime.timedelta(hours=x) for x in range(24)]
while not done:
    string = port.readline().decode("utf-8").strip().replace('\0', '')
    print(string)
    if "end" in string:
        print("end gotted")
        done = True
    else:
        altitude, azimuth = string.split(', ')
        altitudes.append(math.degrees(float(altitude)))
        azimuths.append(math.degrees(float(azimuth)))


hours = [x for x in range(24)]

plt.gca().xaxis.set_major_formatter(mdates.DateFormatter('%H'))
plt.gca().xaxis.set_major_locator(mdates.HourLocator())

plt.plot(dates, altitudes)
plt.gcf().autofmt_xdate()
plt.show()