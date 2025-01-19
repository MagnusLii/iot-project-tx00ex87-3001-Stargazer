#pragma once

#include <string>
#include <vector>
#include <memory>
#include <queue>
#include "message.hpp"
#include "PicoUart.hpp"
#include "pico/stdlib.h"

#define RBUFFER_SIZE 64
#define RWAIT_MS 20


class CommBridge {
  public:
    CommBridge(std::shared_ptr<PicoUart> uart, std::shared_ptr<std::queue<msg::Message>> queue);
    int read(std::string &str);
    void send(const msg::Message &msg);
    void send(const std::string &str);
    int parse(std::string &str);
    int read_and_parse(const uint16_t timeout_ms = 5000, bool reset_on_activity = true);

  private:
    std::shared_ptr<PicoUart> uart;
    std::shared_ptr<std::queue<msg::Message>> queue;
    std::string string_buffer = "";
};
