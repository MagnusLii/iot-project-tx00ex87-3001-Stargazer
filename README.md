<a id="readme-top"></a>

<img src="banner.png" alt="stargazer" style="display:block;margin-left: auto;margin-right: auto;width: 100%"/>

  <p align="center">
    Amateur astronomy device

  </p>


<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
      <ul>
        <li><a href="#built-with">Built With</a></li>
      </ul>
    </li>
    <li>
      <a href="#getting-started">Getting Started</a>
      <ul>
        <li><a href="#prerequisites">Prerequisites</a></li>
        <li><a href="#installation">Installation</a></li>
      </ul>
    </li>
    <li><a href="#usage">Usage</a></li>
    <li><a href="#license">License</a></li>
  </ol>
</details>



<!-- ABOUT THE PROJECT -->
## About The Project

The Stargazer is an easy to set up device that will take pictures of celestial objects at the most opportune times. Stargazer uses a simple and easy to use web interface to drive the device and see the beautiful pictures instantly when they happen. The pictures are also saved locally on an SD card for a convenient way of transferring and handling the pictures. Stargazer can also be driven completely offline using a laptop or a computer, making using the device flexible to use even when you have no internet connectivity.

<p align="right">(<a href="#readme-top">back to top</a>)</p>



### Built With

* [![Pico-SDK][Pico-SDK-badge]][Pico-SDK-url]
* [![ESPidf][ESP-IDF-badge]][ESP-idf-url]
* [![rustup/Cargo][rustup-badge]][rustup-url]

<p align="right">(<a href="#readme-top">back to top</a>)</p>


## Getting Started

This is an example of how you may give instructions on setting up your project locally.
To get a local copy up and running follow these simple example steps.

### Prerequisites

This is an example of how to list things you need to use the software and how to install them.
- ESP-idf: https://github.com/espressif/esp-idf
- Docker: https://www.docker.com/get-started/
- Rust: https://www.rust-lang.org/tools/install

See <a href="Assembly.MD">ASSEMBLY.MD</a> for details on building the physical device

optional:
- Terminal connection software such as putty or minicom

### Installation

1. Clone the repo
   ```sh
   git clone https://gitlab.metropolia.fi/markulii/iot-project-tx00ex87-3001.git
   cd iot-project-tx00ex87-3001
   ```
2. Build Raspberry Pi Pico code with Docker
   ```sh
   cd pico
   docker compose up
   ```
3. Copy the binary to the Raspberry Pi Pico
    ```sh
    cp build/stargazer.uf2 <PATH TO PICO USB DEVICE PATH>
    ```
4. Build and flash ESP<br>
    Set the ESP into flash mode.
    ```sh
    cd esp32-cam
    idf.py -p <PORT> flash
    ```
    Reboot the ESP out of flash mode.
  
5. Build the webserver
   ```sh
   cd webserver
   ```
  - Option A. Build under repository directory
    ```sh
    cargo build --release
    ```
  - Option B. Build and install to Cargo install directory
    ```sh
    cargo install
    ```
   

<p align="right">(<a href="#readme-top">back to top</a>)</p>



## Usage

Run the server
  ```sh
    cargo run
  ```

<a id="direct-control"></a>
### - Operating the device directly:
The device can be operated directly by connecting to the Pico via USB cable and opening a terminal connection with a baud rate of 115200.<br>
Sending any character puts the Pico into command mode which allows for using commands to operate the whole system.<br>


#### List of commands in configuration mode
`help` to see a list of all commands and their arguments.<br>
`exit` to exit command mode.<br>
`heading` to set compass heading of the device<br>
`time` to view or set current time<br>
`coord` to view or set current coordinates<br>
`instruction` to add an instruction directly to device<br>
`wifi` to set wifi details<br>
`server` to set the server details<br>
`token` to set the api token<br>

#### The following additional commands are available if debug build is compiled<br>

`debug_command` to add a command directly to the queue<br>
`debug_picture` to send a take picture message directly to the ESP<br>
`debug_rec_msg` to add a message (as if it came from the ESP) to the receive queue<br>
`debug_send_msg` to send a message to the ESP<br>
`debug_trace` to trace a planet with the device<br>

### Enabling operation from webserver:
When booting the device for the first time you'll need to provide the following for the Stargazer to connecto the webserver and run with full functionality.<br>
 - `Wifi SSID` - The ssid of your wireless network.
 - `Wifi password` - The password of your wireless network.
 - `Webserver domain` - The FQDN or IP address through which the server can be accessed.
 - `Webserver port` - The port used to communicated with the server (default is 8080).
 - `Webserver token` - Token generated in the webserver to enable API calls.
 Optional:
 - `Certificate` - Certificate to enable TLS connections. Only needed if you intend to use TLS to connect to your webserver.

 These settings can be set either by using the appropriate commands in direct control mode see: 
 
 <p align="left"><a href="#direct-control">### - Operating the device directly:</a></p>

  or by setting them in a file called settings.txt on the SDcard connected to the ESP.<br>
  If creating the settings file the settings need to be in the same order as above with each setting on a new line.<br>
  The certificate needs to begin with `-----BEGIN CERTIFICATE-----` and end with `-----END CERTIFICATE-----`.

  IMPORTANT: The final line needs to include a 16bit crc in little endian ordering. A comma is prepended to the crc eg: `,4B37`<br>
  If any setting is missing or if the file is improperly formatted the device will not be able to initialize code/components related to said settings.<br>
  An example `settings.txt` can be found at <a href="example-settings.txt">/example-settings.txt</a> 


### Operating the device from the server:
The command options are as follows:<br>
* Target:<br>
`Sun`<br>`Moon`<br>`Mercury`<br>`Venus`<br>`Mars`<br>`Jupiter`<br>`Saturn`<br>`Uranus`<br>`Neptune`<br>
* Image Position:<br>
`Rising`: Takes a picture when the celestial object rises from the horizon.<br>
`Zenith`: Takes a picture at the zenith of the celestial object.<br>
`Any`: Takes a picture at the current given time.<br>


<p align="right">(<a href="#readme-top">back to top</a>)</p>



<!-- LICENSE -->
## License

Distributed under the ISC License. See `LICENSE` for more information.

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/github_username/repo_name.svg?style=for-the-badge
[contributors-url]: https://github.com/github_username/repo_name/graphs/contributors

[repo-url]: https://gitlab.metropolia.fi/markulii/iot-project-tx00ex87-3001

[Pico-SDK-url]: https://github.com/raspberrypi/pico-sdk
[Pico-SDK-badge]: https://img.shields.io/badge/-RaspberryPi-C51A4A?style=for-the-badge&logo=Raspberry-Pi

[ESP-idf-url]: https://github.com/espressif/esp-idf
[ESP-IDF-badge]: https://img.shields.io/badge/Platform-ESP32-informational

[rustup-url]: https://www.rust-lang.org/tools/install
[rustup-badge]: https://img.shields.io/badge/Rust-%23000000.svg?e&logo=rust&logoColor=white&logoSize=20
