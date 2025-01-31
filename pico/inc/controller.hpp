#pragma once

#include <memory>

#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "gps.hpp"

class Controller {
  public:
    enum State {
        COMM_READ,
        COMM_PROCESS,
        INSTR_PROCESS,
        INSTR_EXECUTE
    };

  public:
    Controller(std::shared_ptr<PicoUart> comm_uart, std::shared_ptr<PicoUart> gps_uart);

    void run();

private:
    void comm_read();
    void comm_process();
    void instr_process();
    void instr_execute();

  private:
    State state;

    std::unique_ptr<Clock> clock;
    std::unique_ptr<GPS> gps;
    std::unique_ptr<Compass> compass;
    std::unique_ptr<CommBridge> commbridge;

    std::shared_ptr<std::queue<msg::Message>> msg_queue;
};
