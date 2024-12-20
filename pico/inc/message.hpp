#pragma once

#include <string>
#include <vector>
#include "convert.hpp"
#include "crc.hpp"
#include <cstdint>

enum MessageType {
    UNASSIGNED = 0,   // don't use
    RESPONSE = 1,     // Response message (ACK/NACK)
    DATETIME = 2,     // Datetime request/response (Pico requests/ESP responds)
    ESP_INIT = 3,     // ESP sends message to Pico when it is initialized
    INSTRUCTIONS = 4, // Receive instructions from ESP
    PICTURE = 5,      // Take picture command to ESP
    DIAGNOSTICS = 6,  // Send diagnostics
};

struct Message {
    MessageType type;
    std::vector<std::string> content;
};

int convert_to_message(std::string &str, Message &msg);
MessageType verify_message_type(std::string &str);
Message response(bool ack);
