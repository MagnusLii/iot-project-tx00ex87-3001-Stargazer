import matplotlib.pyplot as plt
import math

string = """0.284965, 2.35354
0.364602, 2.61631
0.413814, 2.89384
0.427491, 3.17987
0.404086, 3.46453
0.346215, 3.73856
0.259672, 3.99704
0.151686, 4.24026
0.0296049, 4.47240
-0.0995909, 4.69983
-0.229139, 4.93004
-0.352014, 5.17099
-0.455437, 5.41414
-0.542227, 5.69517
-0.597324, 5.99892
-0.613628, 0.0333406
-0.588774, 0.349190
-0.526309, 0.648602
-0.433969, 0.924557
-0.320810, 1.17819
-0.195363, 1.41548
-0.0651825, 1.64414
0.0628352, 1.87208
0.181911, 2.10657"""

altitudes = []
azimuths = []
for line in string.splitlines():
    altitude, azimuth = line.split(', ')
    altitudes.append(math.degrees(float(altitude)))
    azimuths.append(math.degrees(float(azimuth)))

hours = [x for x in range(24)]

plt.plot(hours, altitudes)

plt.show()