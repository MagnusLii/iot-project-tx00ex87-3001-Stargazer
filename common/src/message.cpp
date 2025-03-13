/**
 * @file message.cpp
 * @brief Implementation of message handling functions.
 */

#include "message.hpp"

namespace msg {

/**
 * @brief Converts a string to a Message object.
 *
 * Parses the input string, extracts message content, verifies CRC, and assigns values to the Message struct.
 *
 * @param str Input message string.
 * @param msg Reference to the Message object to populate.
 * @return int Error code (0 if successful, non-zero otherwise).
 */
int convert_to_message(std::string &str, Message &msg) {
    if (size_t pos = str.find_last_of(','); pos == std::string::npos) {
        return 1;
    } else {
        if (str.back() == ';') { str.pop_back(); }
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

/**
 * @brief Determines the message type from a string.
 *
 * @param str Reference to the string containing the message type.
 * @return MessageType The corresponding message type enumeration.
 */
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
        case DEVICE_STATUS:
            return DEVICE_STATUS;
        case INSTRUCTIONS:
            return INSTRUCTIONS;
        case CMD_STATUS:
            return CMD_STATUS;
        case PICTURE:
            return PICTURE;
        case DIAGNOSTICS:
            return DIAGNOSTICS;
        case WIFI:
            return WIFI;
        case SERVER:
            return SERVER;
        case API:
            return API;
        default:
            return UNASSIGNED;
    }
}

/**
 * @brief Validates the CRC of a message.
 *
 * @param str The message content.
 * @param crc_str The CRC value extracted from the message.
 * @return int Error code (0 if valid, non-zero otherwise).
 */
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

/**
 * @brief Converts a Message object to a string representation.
 *
 * @param msg The Message object to convert.
 * @param str Reference to store the output string.
 */
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

/**
 * @brief Creates a response message.
 *
 * @param ack Boolean value indicating acknowledgment.
 * @return Message The response message object.
 */
Message response(bool ack) {
    if (ack) {
        return Message{.type = RESPONSE, .content = {"1"}};
    } else {
        return Message{.type = RESPONSE, .content = {"0"}};
    }
}

/**
 * @brief Creates a datetime request message.
 *
 * @return Message The datetime request message object.
 */
Message datetime_request() { return Message{.type = DATETIME, .content = {"1"}}; }

/**
 * @brief Creates a datetime response message.
 *
 * @param datetime The datetime value to include in the response.
 * @return Message The datetime response message object.
 */
Message datetime_response(int datetime) { return Message{.type = DATETIME, .content = {std::to_string(datetime)}}; }

/**
 * @brief Creates a device status message.
 *
 * @param ok Boolean value indicating device status.
 * @return Message The device status message object.
 */
Message device_status(bool ok) {
    if (ok) {
        return Message{.type = DEVICE_STATUS, .content = {"1"}};
    } else {
        return Message{.type = DEVICE_STATUS, .content = {"0"}};
    }
}

/**
 * @brief Creates an instructions message.
 *
 * @param object_id Object ID.
 * @param image_id Image ID.
 * @param position_id Position ID.
 * @return Message The instructions message object.
 */
Message instructions(int object_id, int image_id, int position_id) {
    return Message{.type = INSTRUCTIONS,
                   .content = {std::to_string(object_id), std::to_string(image_id), std::to_string(position_id)}};
}

/**
 * @brief Creates an instructions message with string parameters.
 *
 * @param object_id Object ID as a string.
 * @param image_id Image ID as a string.
 * @param position_id Position ID as a string.
 * @return Message The instructions message object.
 */
Message instructions(const std::string object_id, const std::string image_id, const std::string position_id) {
    return Message{.type = INSTRUCTIONS, .content = {object_id, image_id, position_id}};
}

/**
 * @brief Creates a command status message.
 *
 * @param image_id Image ID.
 * @param status Command status.
 * @param datetime Datetime.
 * @return Message The command status message object.
 */
Message cmd_status(int image_id, int status, int datetime) {
    return Message{.type = CMD_STATUS,
                   .content = {std::to_string(image_id), std::to_string(status), std::to_string(datetime)}};
}

/**
 * @brief Creates a picture message.
 *
 * @param image_id Image ID.
 * @return Message The picture message object.
 */
Message picture(int image_id) { return Message{.type = PICTURE, .content = {std::to_string(image_id)}}; }

/**
 * @brief Creates a diagnostics message.
 *
 * @param status Diagnostics status.
 * @param diagnostic Diagnostics message.
 * @return Message The diagnostics message object.
 */
Message diagnostics(int status, const std::string diagnostic) {
    return Message{.type = DIAGNOSTICS, .content = {std::to_string(status), diagnostic}};
}

/**
 * @brief Creates a wifi message.
 *
 * @param ssid WiFi SSID.
 * @param password WiFi password.
 * @return Message The wifi message object.
 */
Message wifi(const std::string ssid, const std::string password) {
    return Message{.type = WIFI, .content = {ssid, password}};
}

/**
 * @brief Creates a server message.
 *
 * @param address Server address.
 * @param port Server port.
 * @return Message The server message object.
 */
Message server(const std::string address, int port) {
    return Message{.type = SERVER, .content = {address, std::to_string(port)}};
}

/**
 * @brief Creates an api message.
 *
 * @param api_token API token.
 * @return Message The api message object.
 */
Message api(const std::string api_token) { return Message{.type = API, .content = {api_token}}; }

} // namespace msg
