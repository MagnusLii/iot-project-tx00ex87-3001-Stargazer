#pragma once

#include "convert.hpp"
#include "crc.hpp"
#include <cstdint>
#include <string>
#include <vector>

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

// Used when receiving messages
int convert_to_message(std::string &str, Message &msg);
MessageType verify_message_type(std::string &str);
int check_message_crc(std::string &str, std::string &crc_str);

// Used when sending messages
void convert_to_string(const Message &msg, std::string &str);
Message response(bool ack);
Message datetime_request();
Message datetime_response(/* datetime */);
Message esp_init(bool success);
Message instructions(/* instructions */);
Message picture(int object_id, int image_id);
Message diagnostics(/* diagnostics */);
