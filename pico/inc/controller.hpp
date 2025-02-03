#pragma once

#include <memory>

#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "gps.hpp"

class Controller {
  public:
    enum State {
        SLEEP,
        COMM_READ,
        COMM_PROCESS,
        INSTR_PROCESS,
        MOTOR_CONTROL,
        CAMERA_EXECUTE
    };

  public:
    Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
               std::shared_ptr<CommBridge> commbridge, std::shared_ptr<std::queue<msg::Message>> msg_queue);

    void run();

  private:
    bool init();
    // void comm_read();
    void comm_process();
    void instr_process();
    void motor_control();
    void camera_execute();

  private:
    bool initialized = false;
    State state; // TODO: How do we handle state changes that are external to the loop?
                 // Do we need timers?
                 // ...
    std::queue<msg::Message> instr_msg_queue;


    std::shared_ptr<Clock> clock;
    std::shared_ptr<GPS> gps;
    std::shared_ptr<Compass> compass;
    std::shared_ptr<CommBridge> commbridge;

    std::shared_ptr<std::queue<msg::Message>> msg_queue;
};
