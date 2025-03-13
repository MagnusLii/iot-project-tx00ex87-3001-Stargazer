# Instructions for assembling the Stargazer

## Requirements
- 3D printer
- 8 M3-0.5 heat inserts
- 8 M3 screws
- Electrical tape
- Pico or PicoW (H or WH versions also work)
- AIthinker esp-cam32
- Jumper wires
- 2 grove cables
- 2 28BYJ-48 stepper motors
- 2 6pin Stepper motor drivers
- Crowtail three-axis compass
- Mikroe GPS3 click gps
- Crowtail I2C EEPROM
- 2 TCST1103 opto forks

optional:
- Soldering board(s)

if using nut-lock version of ESP-Holder:<br>
- 3 M3 screws
- 3 M3 nuts

## 3D prints
STL files for printing can be found under `/3D-models/STL`.<br>
Files for 3D models can be found under `/3D-models`.<br>

3D-Models/Gyro-arm-assembly.SLDASM can be used to view the entire assembly as intended to be assembled in solidworks.<br>

Note: There are two files for ESP-holder, both designs work however one locks the ESP more securely. Only one is needed for the Stargazer.

## Assembly

### 3D printed parts and components
Some of the parts are designed to have M3-0.5 heat inserts inserted into them to enable using screws to securely connect the parts.<br>
- The <a href="3D-models/STL/Bottom-bowl.STL">Bottom-bowl</a> is mean to contain most of the electronics and and acts as the stand for the Stargazer.<br>
- The <a href="3D-models/STL/28BYJ-48-bottom-plate.STL">28BYJ-48-Bottom-plate</a> acts as a cover for the bowl and a platform for the camera gyro setup.<br>
Two screw placements are provided to securely mount the plate to the bowl. The plate has a hole for the horizontal bottom motor and opto fork sensor.<br>
There are two insert holes next to the central motor hole to screw the motor onto the plate.<br>
- The <a href="3D-models/STL/Connector-arm.STL">Connector-arm</a> connects the bottom horizontal motor to the <a href="3D-models/STL/28BYJ-48-cover.STL">28BYJ-48-cover</a> which houses the top vertical motor. This connection requres one screw/insert.
- The <a href="3D-models/STL/Opto-fork-holder.STL">Opto-fork-holder</a> is placed ontop of the <a href="3D-models/STL/Connector-arm.STL">Connector-arm</a> and is meant to hold the vertical TCST1103 opto fork. Note our opto forks has custom printed circuit boards which we do not have the files for there fore this <a href="3D-models/STL/Opto-fork-holder.STL">Opto-fork-holder</a> design will not work with most cases.
- And finally the <a href="3D-models/STL/ESP-Holder-Nut-Lock-Design.STL">ESP-Holder</a> connects to the vertical motor without screws and holds the ESP.
- All the other components are located in the bottom bowl.

### Microcontroller wiring
Wiring pins can be seen in <a href="pico/GPIO.md">pico/GPIO.MD</a>, all other devices/motors/sensors are wired directly to the Pico.