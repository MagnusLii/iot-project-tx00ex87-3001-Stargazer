#include "controller.hpp"

#include "debug.hpp"
#include "message.hpp"
#include "sleep_functions.hpp"

#include <algorithm>


bool compare_time(const Command& a, const Command& b) {
    if (a.time.year != b.time.year) return a.time.year < b.time.year;
    if (a.time.month != b.time.month) return a.time.month < b.time.month;
    if (a.time.day != b.time.day) return a.time.day < b.time.day;
    if (a.time.hour != b.time.hour) return a.time.hour < b.time.hour;
    if (a.time.min != b.time.min) return a.time.min < b.time.min;
    return a.time.sec < b.time.sec;
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
            Command command;
            bool done = false;
            bool first = true;
            while (!done) {
                std::cin.getline(buffer + (first ? 1 : 0), 255);
                first = false;
                std::string strong(buffer);
                std::string cmd;
                std::stringstream strung(strong);
                strung >> cmd;
                if (cmd == "d") {
                    int year, month, day, hour, min;
                    strung >> year >> month >> day >> hour >> min;
                    command.time.year = year;
                    command.time.month = month;
                    command.time.day = day;
                    command.time.hour = hour;
                    command.time.min = min;
                    command.time.sec = 0;
                    DEBUG((int)command.time.year, (int)command.time.month, (int)command.time.day, (int)command.time.hour, (int)command.time.min);
                } else if (cmd == "c") {
                    double alti, azi;
                    strung >> alti >> azi;
                    command.coords.altitude = alti * M_PI / 180.0;
                    command.coords.azimuth = azi * M_PI / 180.0;
                    DEBUG(command.coords.altitude, command.coords.azimuth);
                } else if (cmd == "done") {
                    DEBUG("done");
                    done = true;
                    commands.push_back(command);
                    msg::Message mes = msg::instructions(2, 69, 2);
                    instr_msg_queue.push(mes);
                    state = COMM_READ;
                } else if (cmd == "clock") {
                    int toime;
                    strung >> toime;
                    DEBUG(toime);
                    clock->update(toime);
                } else if (cmd == "oc") {
                    double lat, lon;
                    strung >> lat >> lon;
                    DEBUG(lat, lon);
                    #ifdef ENABLE_DEBUG
                    gps->debug_set_coordinates(lat, lon);
                    #endif
                }
            }
        }
        // DEBUG("State: ", state);
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
                    state = COMM_READ;
                    check_motor = false;
                    commbridge->send(msg::picture(image_id));
                    waiting_for_camera = true;
                }
                break;
            case CAMERA_EXECUTE:
                
                break;
            case MOTOR_OFF:
                waiting_for_camera = false;
                mctrl->off();
            case SLEEP: // TODO: Go here after full round of nothing to do?
                // DEBUG("Sleeping");
                if (double_check) {
                    state = COMM_READ;
                } else {
                    // sleep_for(10, 9);
                    if (clock->is_alarm_ringing()) {
                        clock->clear_alarm();
                        state = MOTOR_CALIBRATE;
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

        //result = true; // TODO: Remove
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
            DEBUG((int)commands.front().time.day);
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
