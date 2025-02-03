#pragma once

#include <memory>

#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "gps.hpp"
#include "stepper-motor.hpp"
#include "convert.hpp"
#include "planet_finder.hpp"

// Maybe move this to another file
struct Command {
    uint64_t id;
    Coordinates coords;  
    datetime_t time;
};

class Controller {
  public:
    enum State {
        SLEEP,
        COMM_READ,
        COMM_PROCESS,
        INSTR_PROCESS,
        MOTOR_CONTROL,
        MOTOR_WAIT,
        MOTOR_HOLD,
        MOTOR_OFF,
        CAMERA_EXECUTE,

    };

  public:
    Controller::Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
                       std::shared_ptr<CommBridge> commbridge, std::shared_ptr<StepperMotor> motor_horizontal,
                       std::shared_ptr<StepperMotor> motor_vertical,
                       std::shared_ptr<std::queue<msg::Message>> msg_queue);

    void run();

  private:
    bool init();
    // void comm_read();
    void comm_process();
    void instr_process();
    void motor_control();
    void camera_execute();

  private:
    msg::MessageType last_sent = msg::UNASSIGNED;
    bool initialized = false;
    State state; // TODO: How do we handle state changes that are external to the loop?
                 // Do we need timers?
                 // ...
    std::queue<msg::Message> instr_msg_queue;

    std::vector<Command> commands;


    std::shared_ptr<Clock> clock;
    std::shared_ptr<GPS> gps;
    std::shared_ptr<Compass> compass;
    std::shared_ptr<CommBridge> commbridge;
    std::shared_ptr<StepperMotor> motor_horizontal;
    std::shared_ptr<StepperMotor> motor_vertical;

    std::shared_ptr<std::queue<msg::Message>> msg_queue;
};
