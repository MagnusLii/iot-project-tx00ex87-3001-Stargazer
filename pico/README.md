# Building with Docker

1. Install Docker and Docker Compose
2. Run `docker compose up` in pico directory
3. After the build is complete, the binary will be available at `./build/pico-stargazer.{uf2|elf}` (Assuming it was successful)

# Flashing the Pico

## Using OpenOCD
- Make sure you have a Raspberry Pi Debug Probe or some other CMSIS-DAP device connected, and that you have OpenOCD installed (and in your PATH)
- Run the following command
```
$ openocd -f board/pico-debug.cfg -c "program build/pico-stargazer.elf verify reset exit"
```
- You may need to use `sudo` to run openocd depending on your system


## Copy the binary to the Pico
- Put the Pico into USB mass storage mode by holding down the BOOTSEL button and connecting it to your computer with a USB cable
- Copy the `pico-stargazer.uf2` file to the root of the USB drive
