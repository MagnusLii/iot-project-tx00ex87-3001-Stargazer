#include "commbridge.hpp"

#include "debug.hpp"

CommBridge::CommBridge(std::shared_ptr<PicoUart> uart, std::shared_ptr<std::queue<Message>> queue)
    : uart(uart), queue(queue) {}

int CommBridge::read(std::string &str) {
    uint8_t rbuffer[64] = {0};
    int count = 0;

    while (int len = uart->read(rbuffer, sizeof(rbuffer))) {
        if (rbuffer[0] != 0) {
            str += (char *)rbuffer;
            count += len;
        }
        std::fill_n(rbuffer, sizeof(rbuffer), 0);
        sleep_ms(20);
    }

    return count;
}

void CommBridge::send(const Message &msg) {
    std::string formatted_msg = "";
    convert_to_string(msg, formatted_msg);
    formatted_msg += "\r\n"; // TODO: Remove

    DEBUG("Sending: ", formatted_msg);
    uart->send(formatted_msg.c_str());
}

void CommBridge::send(const std::string &str) {
    std::string formatted_msg = str;
    uint16_t crc = crc16(formatted_msg);
    std::string crc_str;
    num_to_hex_str(crc, crc_str, 4, true, true);
    formatted_msg += "," + crc_str + ";";

    DEBUG("Sending: ", str);
    uart->send(str.c_str());
}

int CommBridge::parse(std::string &str) {
    do {
        // Find the first $ in the string unless a partial message is in string_buffer
        if (string_buffer.empty()) {
            size_t pos;
            if (pos = str.find("$"); pos == std::string::npos) { return 1; }
            str.erase(0, pos);
        }

        // Check if the message is complete and stash if not
        if (size_t pos = str.find(";"); pos == std::string::npos) {
            string_buffer += str;
            str.clear();
        } else {
            string_buffer += str.substr(0, pos);
            str.erase(0, pos + 1);

            DEBUG(string_buffer);
            Message msg;
            if (convert_to_message(string_buffer, msg) == 0) { queue->push(msg); }

            string_buffer.clear();
        }
    } while (!str.empty());

    return 0;
}

int CommBridge::read_and_parse(const uint16_t timeout_ms, bool reset_on_activity) {
    std::string str;
    bool done = false;
    int result = 4;
    uint64_t time = time_us_64();

    while (!done && time_us_64() - time < timeout_ms * 1000) {
        int count = read(str);
        if (count > 0) {
            result = parse(str);
            if (reset_on_activity) { time = time_us_64(); }
        }
        if (result == 0) { done = true; }
    }

    return result;
}
