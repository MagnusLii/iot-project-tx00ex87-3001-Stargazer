#pragma once

#include "pico/stdlib.h"
#include "pico/time.h"

#include "PicoUart.hpp"
#include "message.hpp"

#include <memory>
#include <queue>
#include <string>
#include <vector>

#define RBUFFER_SIZE 64
#define RWAIT_MS     20

/**
 * @brief Handles communication between the Raspberry Pi Pico and the ESP32.
 */
class CommBridge {
  public:
    CommBridge(std::shared_ptr<PicoUart> uart, std::shared_ptr<std::queue<msg::Message>> queue);
    int read(std::string &str);
    void send(const msg::Message &msg);
    void send(const std::string &str);
    int parse(std::string &str);
    int read_and_parse(const uint16_t timeout_ms = 5000, bool reset_on_activity = true);
    bool ready_to_send();

  private:
    absolute_time_t last_sent_time = 0;

    std::shared_ptr<PicoUart> uart;
    std::shared_ptr<std::queue<msg::Message>> queue;
    std::string string_buffer = "";
};
