#include "gps.hpp"

#include "debug.hpp"
#include <hardware/timer.h>

GPS::GPS(std::shared_ptr<PicoUart> uart): uart(uart) {
    
}

int GPS::locate_position(uint16_t timeout_s) {
    uint64_t time = time_us_64();
    bool found = false;
    int empty_reads = 0;
    uint8_t read_buffer[256] = {0};

    do {
        uart->read(read_buffer, sizeof(read_buffer));
        if (read_buffer[0] != 0) {
            std::string str_buffer((char*) read_buffer);
            if (gps_sentence.empty()) {
                // Try to find a start of a GPS sentence
                // By finding a '$' character in the buffer
                if (str_buffer.find('$') != std::string::npos) {
                    gps_sentence = str_buffer.substr(0, str_buffer.find('$') + 1);
                    DEBUG("Found GPS sentence start:", gps_sentence);
                }
            } else {
                std::stringstream gps_stream(str_buffer);


                
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
}


