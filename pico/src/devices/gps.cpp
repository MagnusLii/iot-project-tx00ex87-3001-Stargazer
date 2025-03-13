#include "gps.hpp"

#include "debug.hpp"

/**
 * @file GPS.cpp
 * @brief Implementation of the GPS class for parsing and handling GPS data from a Quectel L80.
 */

/**
 * @brief Constructs a GPS object.
 * @param uart Shared pointer to a PicoUart instance.
 * @param gpgga_on Enables parsing of GPGGA sentences.
 * @param gpgll_on Enables parsing of GPGLL sentences.
 */
GPS::GPS(std::shared_ptr<PicoUart> uart, bool gpgga_on, bool gpgll_on) : gpgga(gpgga_on), gpgll(gpgll_on), uart(uart) {
    uart->flush();
}

/**
 * @brief Attempts to locate the GPS position.
 * @param timeout_s Timeout in seconds.
 * @return Status of GPS fix (1 if successful, 0 otherwise).
 */
int GPS::locate_position(uint16_t timeout_s) {
    uint64_t time = time_us_64();
    int empty_reads = 0;
    uint8_t read_buffer[256] = {0};

    do {
        uart->read(read_buffer, sizeof(read_buffer));
        if (read_buffer[0] != 0) {
            std::string str_buffer((char *)read_buffer);
            parse_output(std::move(str_buffer));
            empty_reads = 0;
            std::fill_n(read_buffer, sizeof(read_buffer), 0);
        } else {
            empty_reads++;
            if (empty_reads % 10 == 0) { DEBUG("Reading nothing from GPS, is it connected?"); }
        }
        sleep_ms(250);
    } while (status == false && time_us_64() - time < timeout_s * 1000000);

    return status;
}

/**
 * @brief Tries to parse an NMEA message from the GPS output.
 * @param output The raw GPS output.
 * @return 0 when done.
 * @note All messages are stored in gps_sentence variable. Incomplete messages can be continued in the next call.
 */
int GPS::parse_output(std::string output) {
    while (!output.empty()) {
        switch (sentence_state) {
            case SentenceState::EMPTY:
                if (size_t pos = output.find("$"); pos != std::string::npos) { output.erase(0, pos); }

                if (size_t pos = output.find("\n"); pos != std::string::npos) {
                    gps_sentence = output.substr(0, pos);
                    gps_sentence.erase(std::remove_if(gps_sentence.begin(), gps_sentence.end(), ::isspace),
                                       gps_sentence.end());
                    output.erase(0, pos + 1);
                    sentence_state = SentenceState::COMPLETE;
                    break;
                } else {
                    gps_sentence = output;
                    output.clear();
                    sentence_state = SentenceState::INCOMPLETE;
                    break;
                }
            case SentenceState::INCOMPLETE:
                if (size_t pos = output.find("\n"); pos != std::string::npos) {
                    gps_sentence += output.substr(0, pos);
                    gps_sentence.erase(std::remove_if(gps_sentence.begin(), gps_sentence.end(), ::isspace),
                                       gps_sentence.end());
                    output.erase(0, pos + 1);
                    sentence_state = SentenceState::COMPLETE;
                } else {
                    gps_sentence += output;
                    output.clear();
                    break;
                }
            case SentenceState::COMPLETE:
                if (gpgga && gps_sentence.find("$GPGGA") != std::string::npos) {
                    DEBUG(gps_sentence);
                    if (parse_gpgga() == 0) { status = true; }
                } else if (gpgll && gps_sentence.find("$GPGLL") != std::string::npos) {
                    DEBUG(gps_sentence);
                    if (parse_gpgll() == 0) { status = true; }
                } else if (gps_sentence.find("$PMTK") != std::string::npos) {
                    DEBUG(gps_sentence);
                } else if (gps_sentence.find("$PQ") != std::string::npos) {
                    DEBUG(gps_sentence);
                }
                gps_sentence.clear();
                sentence_state = SentenceState::EMPTY;
                break;
            default:
                DEBUG("Unknown sentence state");
                break;
        }
    }

    return 0;
}

/**
 * @brief Retrieves the last known coordinates.
 * @return Coordinates structure containing latitude, longitude, and status.
 */
Coordinates GPS::get_coordinates() const { return Coordinates{latitude, longitude, status}; }

/**
 * @brief Sets the GPS mode.
 * @param mode The desired GPS mode.
 */
void GPS::set_mode(Mode mode) {
    if (mode == Mode::FULL_ON) {
        current_mode = Mode::FULL_ON;
        full_on_mode();
    } else if (mode == Mode::STANDBY) {
        current_mode = Mode::STANDBY;
        standby_mode();
    } else if (mode == Mode::ALWAYSLOCATE) {
        current_mode = Mode::ALWAYSLOCATE;
        alwayslocate_mode();
    }
}

/**
 * @brief Retrieves the current GPS mode.
 * @return The current GPS mode.
 */
GPS::Mode GPS::get_mode() const { return current_mode; }

/**
 * @brief Sets the GPS coordinates.
 * @param lat The latitude.
 * @param lon The longitude.
 */
void GPS::set_coordinates(double lat, double lon) {
    latitude = lat;
    longitude = lon;
    status = true;
}

/**
 * @brief Parses GPGGA sentence.
 * @return 0 on success, error code otherwise.
 */
int GPS::parse_gpgga() {
    std::stringstream ss(gps_sentence);
    std::string token;
    std::getline(ss, token, ','); // Message ID
    if (token != "$GPGGA") {
        DEBUG("Not a GPGGA sentence");
        return 1;
    }

    std::getline(ss, token, ','); // UTC time - not used for anything

    std::string nmea_latitude;
    std::getline(ss, nmea_latitude, ',');
    if (nmea_latitude.empty()) {
        DEBUG("Missing latitude");
        return 2;
    }
    std::string ns_indicator;
    std::getline(ss, ns_indicator, ',');
    if (ns_indicator.empty()) {
        DEBUG("Missing NS indicator");
        return 2;
    }
    if (nmea_to_decimal_deg(nmea_latitude, ns_indicator) != 0) {
        DEBUG("Couldn't convert latitude to decimal degrees");
        return 2;
    }

    std::string nmea_longitude;
    std::getline(ss, nmea_longitude, ',');
    if (nmea_longitude.empty()) {
        DEBUG("Missing longitude");
        return 3;
    }
    std::string ew_indicator;
    std::getline(ss, ew_indicator, ',');
    if (ew_indicator.empty()) {
        DEBUG("Missing EW indicator");
        return 3;
    }
    if (nmea_to_decimal_deg(nmea_longitude, ew_indicator) != 0) {
        DEBUG("Couldn't convert longitude to decimal degrees");
        return 3;
    };

    // GPGGA also has a bunch of other stuff we don't need
    //std::getline(ss, token, ','); // Quality
    //std::getline(ss, token, ','); // Number of satellites
    //std::getline(ss, token, ','); // Horizontal dilution of precision
    //std::getline(ss, token, ','); // Altitude
    //std::getline(ss, token, ','); // Altitude units
    //std::getline(ss, token, ','); // Undulation
    //std::getline(ss, token, ','); // Undulation units
    //std::getline(ss, token, ','); // Age of differential GPS data
    //std::getline(ss, token, ','); // Differential reference station ID and checksum

    return 0;
}

/**
 * @brief Parses GPGLL sentence.
 * @return 0 on success, error code otherwise.
 */
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
    if (nmea_latitude.empty()) {
        DEBUG("Missing latitude");
        return 2;
    }
    std::string ns_indicator;
    std::getline(ss, ns_indicator, ',');
    if (ns_indicator.empty()) {
        DEBUG("Missing NS indicator");
        return 2;
    }
    if (nmea_to_decimal_deg(nmea_latitude, ns_indicator) != 0) {
        DEBUG("Couldn't convert latitude to decimal degrees");
        return 2;
    }

    std::string nmea_longitude;
    std::getline(ss, nmea_longitude, ',');
    if (nmea_longitude.empty()) {
        DEBUG("Missing longitude");
        return 3;
    }
    std::string ew_indicator;
    std::getline(ss, ew_indicator, ',');
    if (ew_indicator.empty()) {
        DEBUG("Missing EW indicator");
        return 3;
    }
    if (nmea_to_decimal_deg(nmea_longitude, ew_indicator) != 0) {
        DEBUG("Couldn't convert longitude to decimal degrees");
        return 3;
    };

    return 0;
}


/**
 * @brief Converts NMEA coordinate format to decimal degrees.
 * @param value NMEA coordinate value.
 * @param direction Direction indicator (N, S, E, W).
 * @return 0 on success, error code otherwise.
 */
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

/**
 * @brief Sets the GPS to full-on mode.
 */
void GPS::full_on_mode() {
    const uint8_t pmtk[] = "$PMTK225,0*2B\r\n";
    DEBUG("Sending full-on mode command to GPS");
    uart->write(pmtk, sizeof(pmtk));
}

/**
 * @brief Sets the GPS to standby mode.
 */
void GPS::standby_mode() {
    const uint8_t ptmk[] = "$PMTK161,0*28\r\n";
    DEBUG("Sending standby mode command to GPS");
    uart->write(ptmk, sizeof(ptmk));
    uart->flush();
}

/**
 * @brief Sets the GPS to AlwaysLocate mode.
 */
void GPS::alwayslocate_mode() {
    const uint8_t pmtk[] = "$PMTK225,8*23\r\n";
    DEBUG("Sending AlwaysLocate mode command to GPS");
    uart->write(pmtk, sizeof(pmtk));
}

