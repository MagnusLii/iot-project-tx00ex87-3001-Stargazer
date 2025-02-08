#include "message.hpp"
#include <cstdint>

namespace msg {

int convert_to_message(std::string &str, Message &msg) {
    if (size_t pos = str.find_last_of(','); pos == std::string::npos) {
        return 1;
    } else {
        std::string crc_str = str.substr(pos + 1);
        str.erase(pos);

        if (check_message_crc(str, crc_str)) { return 2; }
    }

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

int check_message_crc(std::string &str, std::string &crc_str) {
    if (crc_str.size() != 4) { return 1; }

    uint16_t attached_crc;
    if (int result; !str_to_int(crc_str, result, true)) {
        return 2;
    } else {
        attached_crc = static_cast<uint16_t>(result);
    }

    if (attached_crc != crc16(str)) { return 3; }

    return 0;
}

void convert_to_string(const Message &msg, std::string &str) {
    str = "$" + std::to_string(msg.type);
    for (auto it = msg.content.begin(); it != msg.content.end(); ++it) {
        str += ",";
        str += it->c_str();
    }

    uint16_t crc = crc16(str);
    std::string crc_str;
    num_to_hex_str(crc, crc_str, 4, true, true);
    str += "," + crc_str + ";";
}

Message response(bool ack) {
    if (ack) {
        return Message{.type = RESPONSE, .content = {"1"}};
    } else {
        return Message{.type = RESPONSE, .content = {"0"}};
    }
}

Message datetime_request() { return Message{.type = DATETIME, .content = {"1"}}; }

Message datetime_response(int datetime) {
    return Message{.type = DATETIME, .content = {std::to_string(datetime)}};
}

Message esp_init(bool success) {
    if (success) {
        return Message{.type = ESP_INIT, .content = {"1"}};
    } else {
        return Message{.type = ESP_INIT, .content = {"0"}};
    }
}

Message instructions(int object_id, int image_id, int position_id) {
    return Message{.type = INSTRUCTIONS, .content = {std::to_string(object_id), std::to_string(image_id), std::to_string(position_id)}};
}

Message instructions(const std::string object_id, const std::string image_id, const std::string position_id) {
    return Message{.type = INSTRUCTIONS, .content = {object_id, image_id, position_id}};
}

Message cmd_status(int image_id, int status, int datetime) {
    return Message{.type = CMD_STATUS, .content = {std::to_string(image_id), std::to_string(status), std::to_string(datetime)}};
}


Message picture(int object_id, int image_id) { // NOTE: Do we actually need to send the object id back to the ESP?
    return Message{.type = PICTURE, .content = {std::to_string(object_id), std::to_string(image_id)}};
}

Message diagnostics(int status, const std::string diagnostic) {
    return Message{.type = DIAGNOSTICS, .content = {std::to_string(status), diagnostic}};
}

Message wifi(std::string ssid, std::string password) {
    return Message{.type = WIFI, .content = {ssid, password}};
}

Message server(std::string server_addr) {
    return Message{.type = SERVER, .content = {server_addr}};
}

Message api(std::string api_token) {
    return Message{.type = API, .content = {api_token}};
}

} // namespace msg
