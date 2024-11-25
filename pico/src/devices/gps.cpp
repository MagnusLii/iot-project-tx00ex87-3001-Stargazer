#include "gps.hpp"

#include "debug.hpp"
#include <cstdint>

GPS::GPS(std::shared_ptr<PicoUart> uart) : uart(uart) { uart->flush(); }

// TODO: Maybe split this up a bit to make it more readable
int GPS::locate_position(uint16_t timeout_s) {
    uint64_t time = time_us_64();
    int empty_reads = 0;
    uint8_t read_buffer[256] = {0};

    status = false;
    do {
        uart->read(read_buffer, sizeof(read_buffer));
        if (read_buffer[0] != 0) {
            std::string str_buffer((char *)read_buffer);
            while (!str_buffer.empty()) {
                // DEBUG(str_buffer);
                if (gps_sentence.empty()) {
                    if (size_t pos = str_buffer.find("$"); pos != std::string::npos) { str_buffer.erase(0, pos); }
                    if (size_t pos = str_buffer.find("\n"); pos != std::string::npos) {
                        gps_sentence = str_buffer.substr(0, pos);
                        str_buffer.erase(0, pos + 1);
                        if (gps_sentence.find("$GPGGA") != std::string::npos) {
                            if (parse_gpgga() == 0) { status = true; }
                        } else if (gps_sentence.find("$GPGLL") != std::string::npos) {
                            if (parse_gpgll() == 0) { status = true; }
                        } else if (gps_sentence.find("$PMTK") != std::string::npos) {
                            if (gps_sentence.find("\r") != std::string::npos) {
                                DEBUG(gps_sentence.substr(0, gps_sentence.find("\r")));
                            }
                            DEBUG(gps_sentence);
                        } else if (gps_sentence.find("$PQ") != std::string::npos) {
                        }
                        gps_sentence.clear();
                    } else {
                        gps_sentence = str_buffer;
                        str_buffer.clear();
                    }
                } else {
                    if (size_t pos = str_buffer.find("\n"); pos != std::string::npos) {
                        gps_sentence += str_buffer.substr(0, pos);
                        str_buffer.erase(0, pos + 1);
                        if (gps_sentence.find("$GPGGA") != std::string::npos) {
                            if (parse_gpgga() == 0) { status = true; }
                        } else if (gps_sentence.find("$GPGLL") != std::string::npos) {
                            if (parse_gpgll() == 0) { status = true; }
                        }
                        gps_sentence.clear();
                    } else {
                        gps_sentence += str_buffer;
                        str_buffer.clear();
                    }
                }
            }

            empty_reads = 0;
            std::fill_n(read_buffer, sizeof(read_buffer), 0);
        } else {
            empty_reads++;
            if (empty_reads % 5 == 0) { DEBUG("Reading nothing from GPS, is it connected?"); }
        }
        sleep_ms(200);

    } while (status == false && time_us_64() - time < timeout_s * 1000000);

    return status;
}

Coordinates GPS::get_coordinates() const { return Coordinates{latitude, longitude, status}; }

// Command doesn't seem to actually do anything..
/*
void GPS::set_nmea_output_frequencies(uint8_t gll, uint8_t rmc, uint8_t vtg, uint8_t gga, uint8_t gsa, uint8_t gsv) {
    if (gll < 0 || gll > 5 || rmc < 0 || rmc > 5 || vtg < 0 || vtg > 5 || gga < 0 || gga > 5 || gsa < 0 || gsa > 5 ||
        gsv < 0 || gsv > 5) {
        DEBUG("Invalid NMEA output frequency");
        return;
    }
    std::string msg = "$PMTK314," + std::to_string(gll) + "," + std::to_string(rmc) + "," + std::to_string(vtg) + "," +
                      std::to_string(gga) + "," + std::to_string(gsa) + "," + std::to_string(gsv) +
                      ",0,0,0,0,0,0,0,0,0,0,0,0,0*29\r\n";
    DEBUG("Sending: " + msg);
    uart->write((uint8_t *)msg.c_str(), msg.length());
}
*/

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

void GPS::set_mode(Mode mode) {
    if (mode == Mode::FULL_ON) {
        full_on_mode();
    } else if (mode == Mode::STANDBY) {
        standby_mode();
    } else if (mode == Mode::ALWAYSLOCATE) {
        alwayslocate_mode();
    }
}

/*
void GPS::set_gptxt_output(bool enable, bool save) {
    std::string pq = "$PQTXT,W," + std::to_string(enable) + "," + std::to_string(save) + "*22\r\n";
    DEBUG("Sending: " + pq);
    uart->write((uint8_t *)pq.c_str(), pq.length());
}

void GPS::full_cold_start() {
    const uint8_t pmtk[] = "$PMTK104*37\r\n";
    DEBUG("Sending:", (char *)pmtk);
    uart->write(pmtk, sizeof(pmtk));
}
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

void GPS::full_on_mode() {
    const uint8_t pmtk[] = "$PMTK225,0*2B\r\n";
    DEBUG("Sending:", (char *)pmtk);
    uart->write(pmtk, sizeof(pmtk));
}

void GPS::standby_mode() {
    const uint8_t ptmk[] = "$PMTK161,0*28\r\n";
    DEBUG("Sending:", (char *)ptmk);
    uart->write(ptmk, sizeof(ptmk));
    uart->flush();
}

void GPS::alwayslocate_mode() {
    const uint8_t pmtk[] = "$PMTK225,8*23\r\n";
    DEBUG("Sending:", (char *)pmtk);
    uart->write(pmtk, sizeof(pmtk));
}

/*
void GPS::reset_system_defaults() {
    const uint8_t pmtk[] = "$PMTK314,-1*04\r\n";
    DEBUG("Sending:", (char *)pmtk);
    uart->write(pmtk, sizeof(pmtk));
}
*/
