#pragma once

#include <memory>

#include "clock.hpp"
#include "commbridge.hpp"
#include "compass.hpp"
#include "convert.hpp"
#include "gps.hpp"
#include "motor-control.hpp"
#include "planet_finder.hpp"
#include "stepper-motor.hpp"
#include "storage.hpp"
#include "structs.hpp"

// #define GPS_COORDS

/**
 * @class Controller
 * @brief Main class for the Pico.
 * @details Handles the main control flow of the Pico.
 */
class Controller {
  public:
    /**
     * @enum State
     * @brief Enumeration of possible states for the Pico.
     */
    enum State {
        SLEEP,
        COMM_READ,
        COMM_SEND,
        CHECK_QUEUES,
        COMM_PROCESS,
        INSTR_PROCESS,
        MOTOR_CALIBRATE,
        MOTOR_CONTROL,
        MOTOR_WAIT,
        MOTOR_OFF,
        TRACE,
    };

  public:
    Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
               std::shared_ptr<CommBridge> commbridge, std::shared_ptr<MotorControl> motor_controller,
               std::shared_ptr<Storage> storage, std::shared_ptr<std::queue<msg::Message>> msg_queue);

    void run();

  private:
    bool init();
    void comm_process();
    void instr_process();
    void config_mode();
    int input(std::string &input, uint32_t timeout, bool hidden = false);
    void wait_for_event(absolute_time_t abs_time, int max_sleep_time);
    bool input_detected();
    void trace();
    bool config_wait_for_response();
    void motor_control();
    void send(const msg::Message mesg);
    void send_process();
    void sanitize_commands();

  private:
    State state = COMM_READ;
    msg::MessageType last_sent = msg::UNASSIGNED;
    Command current_command = {0};
    Command trace_command = {0};
    Celestial trace_object = MOON;
    bool initialized = false;
    bool double_check = true;
    bool check_motor = false;
    bool waiting_for_camera = false;
    bool trace_started = false;
    bool input_received = false;
    bool waiting_for_response = false;
    bool esp_initialized = false;
    bool commands_fetched = false;
    bool trace_pause = true;
    uint64_t trace_time = 0;
    int now_commands = 0;

    std::queue<msg::Message> instr_msg_queue;
    std::queue<msg::Message> send_msg_queue;
    std::vector<Command> commands;

    std::shared_ptr<Clock> clock;
    std::shared_ptr<GPS> gps;
    std::shared_ptr<Compass> compass;
    std::shared_ptr<CommBridge> commbridge;
    std::shared_ptr<MotorControl> mctrl;
    std::shared_ptr<Storage> storage;
    std::shared_ptr<std::queue<msg::Message>> msg_queue;
};
