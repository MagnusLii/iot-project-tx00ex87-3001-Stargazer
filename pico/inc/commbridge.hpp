#pragma once

#include <string>
#include <vector>
#include <memory>
#include <queue>
#include "message.hpp"
#include "PicoUart.hpp"
#include "pico/stdlib.h"

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
