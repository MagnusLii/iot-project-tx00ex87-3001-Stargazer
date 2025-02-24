#include "controller.hpp"

#include "debug.hpp"
#include "message.hpp"
#include "sleep_functions.hpp"

#include <algorithm>


bool compare_time(const Command &first, const Command &second) {
    if (first.time.year < second.time.year) return false;
    else if (first.time.year > second.time.year) return true;
    if (first.time.month < second.time.month) return false;
    else if (first.time.month > second.time.month) return true;
    if (first.time.day < second.time.day) return false;
    else if (first.time.day > second.time.day) return true;
    if (first.time.hour < second.time.hour) return false;
    else if (first.time.hour > second.time.hour) return true;
    if (first.time.min < second.time.min) return false;
    else if (first.time.min > second.time.min) return true;
    return false;
}

Controller::Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
                       std::shared_ptr<CommBridge> commbridge, std::shared_ptr<MotorControl> motor_controller,
                       std::shared_ptr<std::queue<msg::Message>> msg_queue)
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), mctrl(motor_controller), msg_queue(msg_queue) {
    state = COMM_READ;
}

void Controller::run() {
    if (!initialized) {
        DEBUG("Not yet initialized");
        if (init()) {
            initialized = true;
            DEBUG("Initialized");
        } else {
            DEBUG("Failed to initialize");
            return;
        }
    }
    int image_id = 1;  // TODO: ^
    char buffer[256] = {0};
    DEBUG("Starting main loop");
    while (true) {
        if (stdio_get_until(buffer, 1, delayed_by_ms(get_absolute_time(), 50)) != PICO_ERROR_TIMEOUT) {
            std::cout << buffer << std::endl;
            std::cout << "got peek" << std::endl;
        }
        DEBUG("State: ", state);
        switch (state) {
            case COMM_READ:
                double_check = false;
                commbridge->read_and_parse(1000, true);
            case CHECK_QUEUES:
                if (msg_queue->size() > 0) state = COMM_PROCESS;
                else if (instr_msg_queue.size() > 0) state = INSTR_PROCESS;
                else if (check_motor) state = MOTOR_WAIT;
                else if (waiting_for_camera) state = COMM_READ;
                else if (mctrl->isCalibrating()) state = COMM_READ;
                else if (mctrl->isCalibrated()) state = MOTOR_CONTROL;
                else state = SLEEP;
                break;
            case COMM_PROCESS:
                comm_process();
                break;
            case INSTR_PROCESS:
                instr_process();
                break;
            case MOTOR_CALIBRATE:
                    mctrl->calibrate();
                    state = COMM_READ;
                break;
            case MOTOR_CONTROL:
                // TODO: check if its actually the time to do stuff
                mctrl->turn_to_coordinates(commands.front().coords);
                check_motor = true;
            case MOTOR_WAIT:
                if (mctrl->isRunning()) state = COMM_READ;
                else {
                    state = CAMERA_EXECUTE;
                    check_motor = false;
                }
                break;
            case CAMERA_EXECUTE:
                commbridge->send(msg::picture(image_id));
                waiting_for_camera = true;
                break;
            case MOTOR_OFF:
                waiting_for_camera = false;
                mctrl->off();
            case SLEEP: // TODO: Go here after full round of nothing to do?
                DEBUG("Sleeping");
                if (double_check) {
                    state = COMM_READ;
                } else {
                    sleep_for(10, 9);
                    if (clock->is_alarm_ringing()) {
                        clock->clear_alarm();
                        state = MOTOR_CONTROL;
                    } else {
                        state = COMM_READ;
                    }
                    // goes to state COMM_READ OR MOTOR_CONTROL
                    // TODO: Sleeping
                }
                break;
            default:
                DEBUG("Unknown state: ", state);
                state = COMM_READ;
                break;
        }
    }
}

bool Controller::init() {
    DEBUG("Initializing");
    bool result = false;
    int attempts = 0;

    gps->set_mode(GPS::Mode::FULL_ON);
    commbridge->send(msg::datetime_request());
    //compass->calibrate();
    // Motor calibration
    // TODO: use compass to correct azimuth of commands
    while (!result && attempts < 9) {
        if (commbridge->read_and_parse(1000, true) > 0) { comm_process(); }
        gps->locate_position(2);
        if (gps->get_coordinates().status && clock->is_synced()) { result = true; }
        attempts++;

        result = true; // TODO: Remove
    }

    return result;
}

//void Controller::comm_read() {}

void Controller::comm_process() {
    DEBUG("Processing messages");
    while (msg_queue->size() > 0) {
        DEBUG(msg_queue->size());
        msg::Message msg = msg_queue->front();

        switch (msg.type) {
            case msg::RESPONSE: // Received response ACK/NACK from ESP
                DEBUG("Received response");
                if (msg.content[0] == "1") {
                    if (last_sent == msg::PICTURE) {
                        state = MOTOR_OFF;
                    }
                } else {
                    if (last_sent == msg::PICTURE) {
                        state = COMM_READ;
                    }
                }
                break;
            case msg::DATETIME:
                DEBUG("Received datetime");
                clock->update(msg.content[0]);
                break;
            case msg::DEVICE_STATUS: // Send ACK response back to ESP
                DEBUG("Received ESP init");
                commbridge->send(msg::response(true));
                break;
            case msg::INSTRUCTIONS: // Store/Process instructions
                DEBUG("Received instructions");
                instr_msg_queue.push(msg);
                break;
            default:
                DEBUG("Unexpected message type: ", msg.type);
                break;
        }

        msg_queue->pop();
        double_check = true;
        state = SLEEP;
    }
}

void Controller::instr_process() {
    DEBUG("Processing instructions");
    // TODO: Process instructions
    msg::Message instr = instr_msg_queue.front();
    bool error = false;
    if (instr.content.size() == 3 && instr.type == msg::INSTRUCTIONS) {
        int planet_num;
        Planets planet;
        if (str_to_int(instr.content[0], planet_num)) {
            if (planet_num >= 1 && planet_num <= 9) planet = (Planets)planet_num;
            else error = true;
        } else error = true;
        int id;
        if (!str_to_int(instr.content[1], id)) error = true;
        int position;
        if (str_to_int(instr.content[2], position)) {
            if (position < 1 || position > 3) error = true;
        } else error = true;
        if (!error) {
            Celestial celestial(planet);
            celestial.set_observer_coordinates(gps->get_coordinates());
            commands.push_back(celestial.get_interest_point_command((Interest_point)position, clock->get_datetime()));
            // TODO: correct azimuth
            std::sort(commands.begin(), commands.end(), compare_time);
            if (commands.size() > 0) clock->add_alarm(commands[0].time);
        } else {
            DEBUG("Error in instruction.");
        }
    }
    instr_msg_queue.pop();
    double_check = true;
    state = SLEEP;
}

void Controller::motor_control() {
    DEBUG("Controlling motors");
    // TODO: Control motors to adjust the camera in the direction of the object
}
