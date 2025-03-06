#include "commbridge.hpp"

#include "debug.hpp"
#include "message.hpp"
#include <pico/time.h>

using msg::Message;

CommBridge::CommBridge(std::shared_ptr<PicoUart> uart, std::shared_ptr<std::queue<Message>> queue)
    : uart(uart), queue(queue) {}

/*
 * Reads characters from the UART and appends them to a string buffer
 * Returns the number of characters read
 */
int CommBridge::read(std::string &str) {
    uint8_t rbuffer[RBUFFER_SIZE] = {0};
    int count = 0;

    while (int len = uart->read(rbuffer, sizeof(rbuffer))) {
        if (rbuffer[0] != 0) {
            str += (char *)rbuffer;
            count += len;
        }
        std::fill_n(rbuffer, sizeof(rbuffer), 0);
        sleep_ms(RWAIT_MS);
    }

    return count;
}

/*
 * Sends a Message to the UART after formatting it
 */
void CommBridge::send(const Message &msg) {
    std::string formatted_msg = "";
    convert_to_string(msg, formatted_msg);
    formatted_msg += "\r\n"; // TODO: Remove

    send(formatted_msg);
}

/*
 * Sends a string to the UART
 */
void CommBridge::send(const std::string &str) {
    DEBUG("Sending: ", str);
    uart->send(str.c_str());
    last_sent_time = get_absolute_time();
}

/*
 * Parses messages from a string and pushes them to a queue for further processing
 * Returns 1 if nothing useful was found, 0 otherwise
 */
int CommBridge::parse(std::string &str) {
    int parse_count = 0;
    do {
        // Find the first $ in the string unless there is a partial message is in string_buffer
        if (string_buffer.empty()) {
            size_t pos;
            if (pos = str.find("$"); pos == std::string::npos) { return -1; } // Return if no $ is found
            str.erase(0, pos);                                                // Erase everything before the $
        }

        // Check whether the message is complete
        // If it's not complete, add the entire string to string_buffer
        if (size_t pos = str.find(";"); pos == std::string::npos) {
            string_buffer += str;
            str.clear();
        } else { // Message is complete
            string_buffer += str.substr(0, pos);
            str.erase(0, pos + 1);

            Message msg;
            // Try to convert the string to a message and push it to the queue
            if (convert_to_message(string_buffer, msg) == 0) {
                queue->push(msg);
                parse_count++;
            }

            string_buffer.clear();
        }
    } while (!str.empty());

    return parse_count;
}

/*
 * Loop function that reads characters from the UART and parses them until the timeout is reached
 * If reset_on_activity is true, the timeout is reset every time some data is received
 * Returns the number of messages parsed
 */
int CommBridge::read_and_parse(const uint16_t timeout_ms, bool reset_on_activity) {
    std::string str;
    bool done = false;
    int result = 0;
    uint64_t time = time_us_64();

    while (!done && time_us_64() - time < timeout_ms * 1000) {
        int count = read(str);
        if (count > 0) {
            DEBUG(str);
            result = parse(str);
            if (reset_on_activity) { time = time_us_64(); }
        }
        if (result >= 0) { done = true; }
    }

    return result;
}

bool CommBridge::ready_to_send() {
    if (get_absolute_time() - last_sent_time > 10000000) return true; // 10 seconds waited
    return false;
}
