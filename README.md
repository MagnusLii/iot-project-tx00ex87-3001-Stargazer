# Stargazer-Project-TX00EX87-3001


## Requirements
-   PicoSDK 2.0
-   picotool 2.0
-   ESPidf 5.3.1
-   Docker/podman
-   ESP32-cam AIthinker board
-   Pico/PicoW/Pico2/Pico2W board
-   



## ESP & Pico comms
Protocol: UART<br>
Format: Custom Format<br>
example: `$<PREFIX>,<stuff>,<stuff2>,...<CRC>;`<br>
"$" Starts the message, ";" ends the message, `<PREFIX>` is the identifier for data, `<stuffX>` is data value.<br>

Celestial Object IDs:<br>
TBD...<br>

Image position:<br>
1=Rising<br>
2=Zenith<br>
3=Setting<br>

Prefix values:<br>
`<0>` = unassigned/error<br>
`<1>` = ACK<br>
`<2>` = datetime<br>
`<3>` = ESP init message<br>
`<4>` = ESP informs Pico of image to take.<br>
`<5>` = Pico instructs ESP to take image<br>
`<6>` = Diagnostics data from pico.<br>

Stuff to send:

-   Datetime (Pico requests, ESP responds)<br>
    `$<2>,<1>,<CRC>;`<br>
    `$<2>,<Timestamp(int)>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   ESP init ready state message (ESP sends to pico when it's done initializing)<br>
    `$<3>,<bool>,<CRC>;` True = ready, False = ESP is fucked.<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.

-   Image taking<br>
    ESP sends id of object to take picture of.<br>
    Pico sends ack.<br>
    Pico responds when the device is pointed at the celectial object and is ready to take a picture.<br>
    ESP takes picture and sends confirmation to Pico.<br>

    `$<4>,<Celestial object ID (int)>,<Image/command ID (int)>,<position>,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>
    `$<5>,<Celestial object ID (int)>,<Image/command ID (int),<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>

-   Diagnostics data from pico.<br>
    `$<6>,<TBD>,...,<CRC>;`<br>
    `$<1>,<Bool>,<CRC>;` True = ack, False nack.<br>