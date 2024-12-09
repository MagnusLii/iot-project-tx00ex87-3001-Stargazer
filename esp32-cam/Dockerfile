FROM ubuntu:24.04

RUN apt update

RUN DEBIAN_FRONTEND=noninteractive apt -y install git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

RUN git clone --depth 1 -b v5.3.1 --recursive https://github.com/espressif/esp-idf.git /opt/esp-idf

ENV IDF_TOOLS_PATH=/opt/esp-tools
ENV IDF_PATH=/opt/esp-idf

RUN cd /opt/esp-idf \
    && ./install.sh esp32 \