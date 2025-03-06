# Stargazer-Project-TX00EX87-3001


## Requirements
-   PicoSDK 2.1.1
-   Pico Extras 2.1.1
-   picotool 2.1.1
-   ESPidf 5.3.1
-   Docker/podman
-   ESP32-cam AIthinker board
-   Pico/PicoW board



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
`<3>` = Device status check message<br>
`<4>` = ESP relaying command to Pico of what image to take.<br>
`<5>` = Command/Picture status from Pico to ESP<br>
`<6>` = Pico instructs ESP to take image<br>
`<7>` = Diagnostics data from Pico.         
`<8>` = WiFi information from Pico to ESP       
`<9>` = Send server ip/domain + (port) to ESP   
`<10>` = Api token from Pico to ESP             

Diagnostics values:<br>
GPS status?<br>
Compass readings Status?<br>
~~Pico core temp?~~<br>
Watchdog reboot status?<br>
Motor calibration status?<br>
ESP to Uart comm status<br>
Reboot messages<br>


### Messages explained:

-   Datetime (Pico requests, ESP responds)<br>
    `$<2>,<1>,<CRC>;`<br>
    `$<2>,<Timestamp(int)>,<CRC>;` or `$<1>,<Bool>,<CRC>;` nack if not ready to send timestamp.<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   Device init/status state message (ESP/Pico sends and other responds followed by final ack)<br>
    `$<3>,<Bool>,<CRC>;` True = ready, False = fire.<br>
    `$<3>,<Bool>,<CRC>;` True = ready, False = fire.<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   Image taking process<br>
    ESP sends id of object to take picture of.<br>
    Pico sends (n)ack.<br>

    `$<4>,<Celestial object ID (int)>,<Image/command ID (int)>,<Position>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

    Pico sends Command status message to ESP which it relays to the server.<br>
    ESP sends (n)ack.<br>

    `$<5>,<Image/command ID (int)>,<Status (int),<Time (int)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

    _Some time usually passes._<br>

    Pico sends take image message when the device is pointed at the celestial object and it's time to take a picture.<br>
    ESP takes picture and sends confirmation to Pico.<br>

    `$<6>,<Image/command ID (int)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>


-   Diagnostics data from Pico.<br>
    `$<7>,<Status (int)>,<Message (string)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

-   Configuration messages from Pico.<br>

    Wi-Fi details<br>
    `$<8>,<SSID (string)>,<Password (string)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

    Server details<br>
    `$<9>,<IP/domain (string)>,<Port (int)>,<CRC>;` #if port = 0, port is unused<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

    API token<br>
    `$<10>,<Token (string)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>
