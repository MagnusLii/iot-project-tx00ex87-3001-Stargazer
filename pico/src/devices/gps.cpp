#include "gps.hpp"

#include "debug.hpp"

GPS::GPS(std::shared_ptr<PicoUart> uart): uart(uart) {
    
}

int GPS::locate_position(uint16_t timeout_s) {
    uint64_t time = time_us_64();
    int empty_reads = 0;
    uint8_t read_buffer[256] = {0};

    found = false;
    do {
        uart->read(read_buffer, sizeof(read_buffer));
        if (read_buffer[0] != 0) {
            std::string str_buffer((char*) read_buffer);
            if (gps_sentence.empty()) {
                // Try to find a start of a GPS sentence
                // By finding a '$' character in the buffer
                if (size_t pos = str_buffer.find("$"); pos != std::string::npos) {
                    str_buffer.erase(0, pos);
                }
                if (size_t pos = str_buffer.find("\n"); pos != std::string::npos) {
                    gps_sentence = str_buffer.substr(0, pos);
                    parse_gpgga();
                    gps_sentence.clear();
                } else {
                    gps_sentence = str_buffer;
                }
            } else {
                

                
            }
                

            empty_reads = 0;
            std::fill_n(read_buffer, sizeof(read_buffer), 0);
        } else {
            empty_reads++;
            DEBUG("Read nothing from GPS (", empty_reads, ")");
        }


    } while (found == false && time_us_64() - time < timeout_s * 1000000);
    
    return 0;
}

Coordinates GPS::get_coordinates() const {
    // TODO
    //
    // return Coordinates{latitude, longitude};
    return Coordinates{0, 0};
}

int GPS::parse_gpgga() {
    std::stringstream ss(gps_sentence);
    std::string token;
    std::getline(ss, token, ','); // Message ID
    if (token != "$GPGGA") {
        DEBUG("Not a GPGGA sentence");
        return 1;
    }

    std::getline(ss, token, ','); // UTC time

    std::string latitude;
    std::getline(ss, latitude, ',');
    std::string ns_indicator;
    std::getline(ss, ns_indicator, ',');
    convert_to_decimal_deg(latitude, ns_indicator);

    std::string longitude;
    std::getline(ss, longitude, ',');
    std::string ew_indicator;
    std::getline(ss, ew_indicator, ',');
    convert_to_decimal_deg(longitude, ew_indicator);

    std::getline(ss, token, ',');


    return 0;
}

int GPS::convert_to_decimal_deg(const std::string& value, const std::string& direction) {
    if (value.empty() || direction.empty()) {
        return 1;
    }

    size_t deg_len = (value.find('.') > 2) ? 2 : 3;
    double degrees = std::stod(value.substr(0, deg_len));
    double minutes = std::stod(value.substr(deg_len));

    double decimal_degrees = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W") {
        decimal_degrees *= -1;
    }

    if (direction == "S" || direction == "N") {
        latitude = decimal_degrees;
    } else if (direction == "E" || direction == "W") {
        longitude = decimal_degrees;
    }

    return 0;
}
