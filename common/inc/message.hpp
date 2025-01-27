#pragma once

#include "convert.hpp"
#include "crc.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace msg {

enum MessageType {
    UNASSIGNED = 0,   // don't use
    RESPONSE = 1,     // Response message (ACK/NACK)
    DATETIME = 2,     // Datetime request/response (Pico requests/ESP responds)
    ESP_INIT = 3,     // ESP sends message to Pico when it is initialized
    INSTRUCTIONS = 4, // Receive instructions from ESP
    CMD_STATUS = 5,   // Command status
    PICTURE = 6,      // Take picture command to ESP
    DIAGNOSTICS = 7,  // Send diagnostics
    WIFI = 8,         // Send wifi info
    SERVER = 9,       // Send server info
    API = 10,         // Send api token
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
Message datetime_response(int datetime);
Message esp_init(bool success);
Message instructions(int object_id, int image_id, int position_id);
Message cmd_status(int image_id, int status, int datetime);
Message picture(int object_id, int image_id);
Message diagnostics(/* diagnostics */);
Message wifi(std::string ssid, std::string password);
Message server(std::string server_addr);
Message api(std::string api_token);

} // namespace msg
