import matplotlib.pyplot as plt
import math
import serial



port = serial.Serial('/dev/ttyACM0', 115200)
done = False

altitudes = []
azimuths = []
print("reading...")
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

plt.plot(hours, altitudes)
plt.show()