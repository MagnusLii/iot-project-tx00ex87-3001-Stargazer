#include "message.hpp"

int convert_to_message(std::string &str, Message &msg) {
    std::vector<std::string> tokens;
    if (!str_to_vec(str, ',', tokens)) { return 1; }

    // TODO: CHECK FOR CRC/CHECKSUM

    msg.type = verify_message_type(tokens[0]);
    if (msg.type == UNASSIGNED) { return 2; }

    tokens.erase(tokens.begin());
    msg.content = tokens;

    str.clear();

    return 0;
}

MessageType verify_message_type(std::string &str) {
    int type_val;
    if (!str_to_int(str, type_val)) return UNASSIGNED;

    switch (type_val) {
        case RESPONSE:
            return RESPONSE;
        case DATETIME:
            return DATETIME;
        case ESP_INIT:
            return ESP_INIT;
        case INSTRUCTIONS:
            return INSTRUCTIONS;
        case PICTURE:
            return PICTURE;
        case DIAGNOSTICS:
            return DIAGNOSTICS;
        default:
            return UNASSIGNED;
    }
}

Message response(bool ack) {
    Message msg;
    msg.type = RESPONSE;
    msg.content.push_back(std::to_string(ack));
    return msg;
}

