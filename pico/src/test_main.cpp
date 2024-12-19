#include "hardware/clock.hpp"
#include "pico/stdlib.h"
#include "uart/PicoUart.hpp"
#include <hardware/timer.h>
#include <iostream>
#include <memory>
#include <queue>
#include <sstream>
#include <vector>

#include "debug.hpp"

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

class CommBridge {
  public:
    CommBridge(std::shared_ptr<PicoUart> uart, std::shared_ptr<std::queue<Message>> queue);
    int read(std::string &str);
    void send(const Message &msg);
    int parse(std::string &str);
    int read_and_parse(const uint16_t timeout_ms = 5000, bool reset_on_activity = true);

  private:
    std::shared_ptr<PicoUart> uart;
    std::shared_ptr<std::queue<Message>> queue;
    std::string string_buffer = "";
};

uint8_t checksum8(const std::string &input) {
    uint32_t sum = 0;

    for (char c : input) {
        sum += static_cast<unsigned char>(c);
    }

    return static_cast<uint8_t>(sum % 256);
}

// Convert string to integer with error checking
bool str_to_int(std::string &str, int &result) {
    std::stringstream ss(str);
    if (!(ss >> result)) { return false; }
    return true;
}

bool str_to_vec(const std::string &str, const char delim, std::vector<std::string> &vec) {
    std::stringstream ss(str);
    std::string token;
    bool result = false;

    while (std::getline(ss, token, delim)) {
        vec.push_back(token);
    }

    if (!vec.empty()) result = true;

    return result;
}

// String should contain a number corresponding to the message type
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
        sleep_ms(100);
    }

    return count;
}

void CommBridge::send(const Message &msg) {
    std::string formatted_msg = "$" + std::to_string(msg.type);
    for (auto it = msg.content.begin(); it != msg.content.end(); ++it) {
        formatted_msg += ",";
        formatted_msg += it->c_str();
    }
    formatted_msg += ";";

    formatted_msg += "\r\n"; // TODO: Remove

    DEBUG("Sending: ", formatted_msg);
    uart->send(formatted_msg.c_str());
}

int CommBridge::parse(std::string &str) {
    do {
        // Find the first $ in the string unless a partial message is in string_buffer
        if (string_buffer.empty()) {
            size_t pos;
            if (pos = str.find("$"); pos == std::string::npos) { return 1; }
            str.erase(0, pos + 1);
            if (str.empty()) { str = " "; }
        }

        // Check if the message is complete and stash if not
        if (size_t pos = str.find(";"); pos != std::string::npos) {
            string_buffer += str.substr(0, pos);
            str.erase(0, pos + 1);
        } else {
            string_buffer += str;
            str.clear();
            return 2;
        }

        uart->send("String buffer: ");
        uart->send(string_buffer.c_str());
        uart->send("\r\n");

        Message msg;
        if (int result = convert_to_message(string_buffer, msg) != 0) {
            uart->send("Failed to convert to message: ");
            uart->send(std::to_string(result).c_str());
            uart->send("\r\n");
            return 3; }

        uart->send("Received message ");
        uart->send(std::to_string(msg.type).c_str());
        uart->send("\r\n");
        queue->push(msg);
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
        sleep_ms(100);
    }
    // string_buffer.clear();

    return result;
}

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

Message response(bool ack) {
    Message msg;
    msg.type = RESPONSE;
    msg.content.push_back(std::to_string(ack));
    return msg;
}

int main() {
    stdio_init_all();
    sleep_ms(5000);
    auto queue = std::make_shared<std::queue<Message>>();
    auto uart = std::make_shared<PicoUart>(0, 0, 1, 9600);

    auto clock = std::make_shared<Clock>();

    CommBridge bridge(uart, queue);

    int count = 0;
    for (;;) {
        if (count % 2 == 0) {
            uart->send("Sleepy time\r\n");
        } else {
            uart->send("Received wakeup signal\r\n");
            bridge.read_and_parse(10000);
        }

        while (queue->size() > 0) {
            Message msg = queue->front();
            queue->pop();
            uart->send("Received message ");
            uart->send(std::to_string(msg.type).c_str());
            uart->send("\r\n");
            switch (msg.type) {
                case RESPONSE: // Received response ACK/NACK from ESP
                    break;
                case DATETIME:
                    clock->update(msg.content[0]);
                    break;
                case ESP_INIT: // Send ACK response back to ESP
                    bridge.send(response(true));
                    break;
                case INSTRUCTIONS:
                    // TODO: Handle received instructions
                    break;
                case PICTURE:
                    // TODO: Handle taking picture
                    break;
                case DIAGNOSTICS: // Pico should not receive these
                    DEBUG("Received diagnostics for some reason");
                    break;
                default:
                    DEBUG("Unknown message type: ", msg.type);
                    break;
            }
        }

        count++;
        sleep_ms(2000);
    }
    return 0;
}
