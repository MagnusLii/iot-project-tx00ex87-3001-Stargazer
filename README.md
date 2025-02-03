# Stargazer-Project-TX00EX87-3001


## Requirements
-   PicoSDK 2.0
-   picotool 2.0
-   ESPidf 5.3.1
-   Docker/podman
-   ESP32-cam AIthinker board
-   Pico/PicoW board
-   



## ESP & Pico comms
Protocol: UART<br>
Format: Custom Format<br>
example: `$<PREFIX>,<stuff>,<stuff2>,...<CRC>;`<br>
"$" Starts the message, ";" ends the message, `<PREFIX>` is the identifier for data, `<stuffX>` is data value.<br>

Celestial Object IDs:<br>
`<1>` = Sun<br>
`<2>` = Moon<br>
`<3>` = Mercury<br>
`<4>` = Venus<br>
`<5>` = Mars<br>
`<6>` = Jupiter<br>
`<7>` = Saturn<br>
`<8>` = Uranus<br>
`<9>` = Neptune<br>

Image position:<br>
`<1>` = Rising<br>
`<2>` = Zenith<br>
`<3>` = Setting<br>

Prefix values:<br>
`<0>` = unassigned/error<br>
`<1>` = ACK<br>
`<2>` = Datetime<br>
`<3>` = ESP init message<br>
`<4>` = ESP informs Pico of image to take.<br>
`<5>` = Command/Picture status from Pico to ESP<br>
`<6>` = Pico instructs ESP to take image<br>
`<7>` = Diagnostics data from pico.<br>
`<8>` = WiFi information from Pico to ESP<br>
`<9>` = Send server ip/domain + (port) to ESP<br>
`<10>` = Api token from Pico to ESP<br>

Diagnostics values:<br>
GPS status?<br>
Compass readings Status?<br>
Pico core temp?<br>
Watchdog reboot status?<br>
Motor calibration status?<br>
ESP to Uart comm status<br>
Reboot messages<br>


Stuff to send:

-   Datetime (Pico requests, ESP responds)<br>
    `$<2>,<1>,<CRC>;`<br>
    `$<2>,<Timestamp(int)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   ESP init ready state message (ESP sends to pico when it's done initializing)<br>
    `$<3>,<Bool>,<CRC>;` True = ready, False = ESP is fucked.<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   Image taking<br>
    ESP sends id of object to take picture of.<br>
    Pico sends ack.<br>
    Pico responds when the device is pointed at the celectial object and is ready to take a picture.<br>
    ESP takes picture and sends confirmation to Pico.<br>

    `$<4>,<Celestial object ID (int)>,<Image/command ID (int)>,<Position>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>
    `$<5>,<Celestial object ID (int)>,<Image/command ID (int),<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

-   Diagnostics data from pico.<br>
    `$<6>,<TBD>,...,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>