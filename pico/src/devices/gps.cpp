#include "gps.hpp"

#include "debug.hpp"

GPS::GPS(std::shared_ptr<PicoUart> uart) : uart(uart) {}

int GPS::locate_position(uint16_t timeout_s) {
    uint64_t time = time_us_64();
    int empty_reads = 0;
    uint8_t read_buffer[256] = {0};

    status = false;
    do {
        uart->read(read_buffer, sizeof(read_buffer));
        if (read_buffer[0] != 0) {
            std::string str_buffer((char *)read_buffer);
            if (gps_sentence.empty()) {
                if (size_t pos = str_buffer.find("$"); pos != std::string::npos) { str_buffer.erase(0, pos); }
                if (size_t pos = str_buffer.find("\n"); pos != std::string::npos) {
                    gps_sentence = str_buffer.substr(0, pos);
                    if (gps_sentence.find("$GPGGA") != std::string::npos) {
                        if (parse_gpgga() == 0) { status = true; }
                    } else if (gps_sentence.find("$GPGLL") != std::string::npos) {
                        if (parse_gpgll() == 0) { status = true; }
                    }
                    gps_sentence.clear();
                } else {
                    gps_sentence = str_buffer;
                }
            } else {
                if (size_t pos = str_buffer.find("\n"); pos != std::string::npos) {
                    gps_sentence += str_buffer.substr(0, pos);
                    if (gps_sentence.find("$GPGGA") != std::string::npos) {
                        if (parse_gpgga() == 0) { status = true; }
                    } else if (gps_sentence.find("$GPGLL") != std::string::npos) {
                        if (parse_gpgll() == 0) { status = true; }
                    }
                    gps_sentence.clear();
                } else {
                    gps_sentence += str_buffer;
                }
            }

            empty_reads = 0;
            std::fill_n(read_buffer, sizeof(read_buffer), 0);
        } else {
            empty_reads++;
            DEBUG("Read nothing from GPS, is it connected? (", empty_reads, ")");
        }

    } while (status == false && time_us_64() - time < timeout_s * 1000000);

    return 0;
}

Coordinates GPS::get_coordinates() const { return Coordinates{latitude, longitude, status}; }

int GPS::parse_gpgga() {
    std::stringstream ss(gps_sentence);
    std::string token;
    std::getline(ss, token, ','); // Message ID
    if (token != "$GPGGA") {
        DEBUG("Not a GPGGA sentence");
        return 1;
    }

    std::getline(ss, token, ','); // UTC time
    // TODO: Do we want to do anything with this?

    std::string nmea_latitude;
    std::getline(ss, nmea_latitude, ',');
    std::string ns_indicator;
    std::getline(ss, ns_indicator, ',');
    nmea_to_decimal_deg(nmea_latitude, ns_indicator);

    std::string nmea_longitude;
    std::getline(ss, nmea_longitude, ',');
    std::string ew_indicator;
    std::getline(ss, ew_indicator, ',');
    nmea_to_decimal_deg(nmea_longitude, ew_indicator);

    std::getline(ss, token, ','); // Quality
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Number of satellites
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Horizontal dilution of precision
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Altitude
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Altitude units
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Undulation
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Undulation units
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Age of differential GPS data
    // TODO: Do we want to do anything with this?

    std::getline(ss, token, ','); // Differential reference station ID and checksum
    // TODO: Do we want to do anything with this?

    return 0;
}

int GPS::parse_gpgll() {
    std::stringstream ss(gps_sentence);
    std::string token;
    std::getline(ss, token, ','); // Message ID
    if (token != "$GPGLL") {
        DEBUG("Not a GPGLL sentence");
        return 1;
    }

    std::string nmea_latitude;
    std::getline(ss, nmea_latitude, ',');
    std::string ns_indicator;
    std::getline(ss, ns_indicator, ',');
    if (nmea_to_decimal_deg(nmea_latitude, ns_indicator) != 0) {
        DEBUG("Couldn't convert latitude to decimal degrees");
        return 2;
    }

    std::string nmea_longitude;
    std::getline(ss, nmea_longitude, ',');
    std::string ew_indicator;
    std::getline(ss, ew_indicator, ',');
    if (nmea_to_decimal_deg(nmea_longitude, ew_indicator) != 0) {
        DEBUG("Couldn't convert longitude to decimal degrees");
        return 3;
    };

    return 0;
}

int GPS::nmea_to_decimal_deg(const std::string &value, const std::string &direction) {
    if (value.empty() || direction.empty()) { return 1; }

    size_t deg_len = value.find('.') - 2;
    if (deg_len != 2 && deg_len != 3) {
        DEBUG("Invalid value for lon/lat");
        return 2;
    }

    double degrees = std::stod(value.substr(0, deg_len));
    double minutes = std::stod(value.substr(deg_len));

    double decimal_degrees = degrees + (minutes / 60.0);

    if (direction == "S" || direction == "W") { decimal_degrees *= -1; }

    if (direction == "N" || direction == "S") {
        latitude = decimal_degrees;
    } else if (direction == "E" || direction == "W") {
        longitude = decimal_degrees;
    } else {
        DEBUG("Invalid direction");
        return 3;
    }

    return 0;
}
