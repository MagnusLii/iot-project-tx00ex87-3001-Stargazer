#include "message.hpp"

int convert_to_message(std::string &str, Message &msg) {
    // TODO: To separate function?
    if (size_t pos = str.find_last_of(','); pos == std::string::npos) {
        return 1;
    } else {
        std::string crc_str = str.substr(pos + 1);
        str.erase(pos);
        // str.erase(pos + 1);
        if (crc_str.size() != 4) { return 2; }

        uint16_t attached_crc;
        if (int result; !str_to_int(crc_str, result, true)) {
            return 3;
        } else {
            attached_crc = static_cast<uint16_t>(result);
        }

        if (attached_crc != crc16(str)) { return 4; }
    }
    // END TODO

    std::vector<std::string> tokens;
    if (!str_to_vec(str, ',', tokens)) { return 5; }

    if (tokens.size() < 2) { return 6; }

    msg.type = verify_message_type(tokens[0]);
    if (msg.type == UNASSIGNED) { return 7; }

    tokens.erase(tokens.begin());
    msg.content = tokens;

    str.clear();

    return 0;
}

MessageType verify_message_type(std::string &str) {
    if (str[0] != '$') {
        return UNASSIGNED;
    } else {
        str.erase(0, 1);
    }
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
