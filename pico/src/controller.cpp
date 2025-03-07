#include "controller.hpp"

#include "convert.hpp"
#include "debug.hpp"
#include "message.hpp"

#include <algorithm>
#include <cctype>
#include <hardware/timer.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>

bool compare_time(const Command &a, const Command &b) {
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
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), mctrl(motor_controller), msg_queue(msg_queue) {}

void Controller::run() {
    if (input_detected()) {
        config_mode();
        DEBUG("Exited config mode: init");
    }
    if (!initialized) {
        DEBUG("Not yet initialized");
        if (init()) {
            initialized = true;
            gps->set_mode(GPS::Mode::STANDBY);
            commbridge->send(msg::device_status(true));
            last_sent = msg::DEVICE_STATUS;
            waiting_for_response = true;
            DEBUG("Initialized");
        } else {
            DEBUG("Failed to initialize");
            return;
        }
    }

    DEBUG("Starting main loop");
    while (true) {
        if (input_received || input_detected()) {
            config_mode();
            DEBUG("Exited config mode: main loop");
        }

        // DEBUG("State: ", state);
        switch (state) {
            case COMM_READ:
                double_check = false;
                commbridge->read_and_parse(1000, true);
            case CHECK_QUEUES:
                if (msg_queue->size() > 0)
                    state = COMM_PROCESS;
                else if (instr_msg_queue.size() > 0)
                    state = INSTR_PROCESS;
                else if (check_motor)
                    state = MOTOR_WAIT;
                else if (waiting_for_camera)
                    state = COMM_READ;
                else if (mctrl->isCalibrating())
                    state = COMM_READ;
                else if (trace_started)
                    state = TRACE;
                else if (mctrl->isCalibrated())
                    state = MOTOR_CONTROL;
                else
                    state = SLEEP;
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
                if (commands.size() > 0) {
                    if (commands.front().time.hour == clock->get_datetime().hour) {
                        current_command =
                            commands.front(); // TODO: Command needs to removed from vector after it's done
                        mctrl->turn_to_coordinates(current_command.coords);
                        check_motor = true;
                    }
                }
                state = MOTOR_WAIT;
                break;
            case MOTOR_WAIT:
                if (mctrl->isRunning())
                    state = COMM_READ;
                else {
                    state = COMM_READ;
                    check_motor = false;
                    int image_id = 1; // TODO: Replace with actual image id
                    commbridge->send(msg::picture(image_id));
                    last_sent = msg::PICTURE;
                    waiting_for_response = true;
                    waiting_for_camera = true;
                }
                break;
            case TRACE:
                trace();
                break;
            case MOTOR_OFF:
                waiting_for_camera = false;
                mctrl->off();
                state = SLEEP;
                break;
            case SLEEP:
                if (double_check) {
                    state = COMM_READ;
                } else {
                    DEBUG("Sleeping");
                    wait_for_event(get_absolute_time(), 30000000); // 30s TODO: Replace with actual max sleep time
                    DEBUG("Waking up");
                    if (clock->is_alarm_ringing()) {
                        clock->clear_alarm();
                        state = MOTOR_CALIBRATE;
                    } else {
                        state = COMM_READ;
                    }
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

    if (!gps->get_coordinates().status) {
        if (gps->get_mode() != GPS::Mode::FULL_ON) gps->set_mode(GPS::Mode::FULL_ON);
    }
    if (!clock->is_synced()) {
        if (waiting_for_response && commbridge->ready_to_send()) {
            commbridge->send(msg::datetime_request());
        } else if (!waiting_for_response) {
            commbridge->send(msg::datetime_request());
            waiting_for_response = true;
        }
    }
    compass_heading = compass->getHeading();
    DEBUG("Got compass heading:", compass_heading);
    //  TODO: use compass to correct azimuth of commands
    if (commbridge->read_and_parse(1000, true) > 0) { comm_process(); }
    if (!gps->get_coordinates().status) gps->locate_position(2);

    if (gps->get_coordinates().status && clock->is_synced()) { result = true; }
    DEBUG("GPS fix status:", gps->get_coordinates().status);
    DEBUG("Clock sync status:", clock->is_synced());

    return result;
}

void Controller::comm_process() {
    DEBUG("Processing messages");
    while (msg_queue->size() > 0) {
        DEBUG(msg_queue->size());
        msg::Message msg = msg_queue->front();

        waiting_for_response = false; // TODO: What are the places we should check this?
        switch (msg.type) {
            case msg::RESPONSE: // Received response ACK/NACK from ESP
                DEBUG("Received response");
                if (msg.content[0] == "1") {
                    if (last_sent == msg::PICTURE) { state = MOTOR_OFF; }
                } else {
                    if (last_sent == msg::PICTURE) { state = COMM_READ; }
                } // TODO: Handle other responses?
                break;
            case msg::DATETIME:
                DEBUG("Received datetime");
                clock->update(msg.content[0]);
                commbridge->send(msg::response(true));
                last_sent = msg::RESPONSE;
                break;
            case msg::DEVICE_STATUS: // Send ACK or DEVICE_STATUS response back to ESP
                DEBUG("Received ESP init");
                if (msg.content[0] == "1") {
                    esp_initialized = true;
                } else {
                    esp_initialized = false;
                }
                if (last_sent == msg::DEVICE_STATUS) {
                    commbridge->send(msg::response(true));
                    last_sent = msg::RESPONSE;
                } else {
                    commbridge->send(msg::device_status(true));
                    last_sent = msg::DEVICE_STATUS;
                    waiting_for_response = true;
                }
                break;
            case msg::INSTRUCTIONS: // Store/Process instructions
                DEBUG("Received instructions");
                instr_msg_queue.push(msg);
                commbridge->send(msg::response(true));
                last_sent = msg::RESPONSE;
                break;
            default:
                DEBUG("Unexpected message type: ", msg.type);
                commbridge->send(msg::response(false));
                last_sent = msg::RESPONSE;
                break;
        }

        msg_queue->pop();
        double_check = true;
        state = SLEEP;
    }
}

void Controller::instr_process() {
    DEBUG("Processing instructions");
    msg::Message instr = instr_msg_queue.front();
    bool error = false;
    Planets planet = MOON;
    if (instr.content.size() == 3 && instr.type == msg::INSTRUCTIONS) {
        int planet_num;
        if (str_to_int(instr.content[0], planet_num)) {
            if (planet_num >= 1 && planet_num <= 9)
                planet = (Planets)planet_num;
            else
                error = true;
        } else
            error = true;

        int id; // TODO: Needs to be saved to Command
        if (!str_to_int(instr.content[1], id)) error = true;

        int position;
        if (str_to_int(instr.content[2], position)) {
            if (position < 1 || position > 3) error = true;
        } else
            error = true;

        if (!error) {
            Celestial celestial(planet);
            celestial.set_observer_coordinates(gps->get_coordinates());
            Command command = celestial.get_interest_point_command((Interest_point)position, clock->get_datetime());
            if (command.time.year < 2000) {
                DEBUG("Instruction not possible");
                commbridge->send(msg::cmd_status(id, -2, 0));
                last_sent = msg::CMD_STATUS;
                waiting_for_response = true;
                return;
            }
            commbridge->send(msg::cmd_status(id, 2,
                                             datetime_to_epoch(command.time.year, command.time.month, command.time.day,
                                                               command.time.hour, command.time.min, command.time.sec)));
            last_sent = msg::CMD_STATUS;
            waiting_for_response = true;
            commands.push_back(command);
            // TODO: correct azimuth
            std::sort(commands.begin(), commands.end(), compare_time);
            DEBUG("Next command: ", (int)commands.front().time.year, (int)commands.front().time.month,
                  (int)commands.front().time.day, (int)commands.front().time.hour, (int)commands.front().time.min);
            if (commands.size() > 0) clock->add_alarm(commands.front().time);
        } else {
            DEBUG("Error in instruction.");
            // TODO: should we send back error message?
            commbridge->send(msg::cmd_status(id, -1, 0));
            last_sent = msg::CMD_STATUS;
            waiting_for_response = true;
        }
    }

    instr_msg_queue.pop();
    double_check = true;
    state = SLEEP;
}

void Controller::config_mode() {
    const int64_t TIMEOUT = 60000000;

    DEBUG("Stdio input detected. Entering config mode...");
    input_received = false;
    std::string input_buffer = "";
    bool exit = false;

    std::cout << "Stargazer config mode - type \"help\" for available commands";
    if (waiting_for_response && waiting_for_camera) {
        std::cout << std::endl
                  << "! Device is currently waiting for response from ESP..." << std::endl
                  << "! Please avoid using send commands (wifi|server|token|debug_picture|debug_send_msg)"
                  << " while waiting for response" << std::endl;
    }
    while (!exit) {
        std::cout << std::endl << "> ";
        std::cout.flush();

        int rc = input(input_buffer, TIMEOUT);
        if (rc == -1) {
            exit = true;
            std::cout << "Exiting config mode" << std::endl;
        }

        if (rc > 0) {
            // DEBUG(input);
            std::stringstream ss(input_buffer);
            std::string token;
            ss >> token;
            if (token == "help") {
                std::cout << "Available commands:" << std::endl
                          << "help - print this help message" << std::endl
                          << "exit - exit config mode" << std::endl
                          << "time [unixtime] - view or set current time" << std::endl
                          << "coord [<lat> <lon>] - view or set current coordinates" << std::endl
                          << "instruction <object_id> <command_id> <position_id> - add an instruction to the queue"
                          << std::endl
                          << "wifi <ssid> - set wifi details. You will be prompted for the password" << std::endl
                          << "server <host> <port> - set the server details" << std::endl
                          << "token <token> - set the server api token" << std::endl
#ifdef ENABLE_DEBUG
                          << "debug_command <year> <month> <day> <hour> <min> <alt> <azi> - add a command directly "
                             "to the queue"
                          << std::endl
                          << "debug_picture <image_id> - send a take picture message to the ESP" << std::endl
                          << "debug_rec_msg <message_str> - add a message to the receive queue" << std::endl
                          << "debug_send_msg <message_type> <message_content_1> ... - send a message to the ESP"
                          << std::endl
                          << "debug_trace <planet_id> - trace a planet" << std::endl
#endif
                    ;
            } else if (token == "exit") {
                exit = true;
                std::cout << "Exiting config mode" << std::endl;
            } else if (token == "time") {
                time_t timestamp = 0;
                if (ss >> timestamp) {
                    clock->update(timestamp);
                    datetime_t now = clock->get_datetime();
                    std::cout << "Time set to " << now.year << "-" << +now.month << "-" << +now.day << " " << +now.hour
                              << ":" << +now.min << std::endl;
                } else {
                    datetime_t now = clock->get_datetime();
                    std::cout << "Time is " << now.year << "-" << +now.month << "-" << +now.day << " " << +now.hour
                              << ":" << +now.min << std::endl;
                }
            } else if (token == "coord") {
                double lat, lon;
                if (ss >> lat >> lon) {
                    gps->set_coordinates(lat, lon);
                    std::cout << "Coordinates set to " << lat << ", " << lon << std::endl;
                } else {
                    Coordinates coords = gps->get_coordinates();
                    if (!coords.status) {
                        std::cout << "Coordinates are not available" << std::endl;
                    } else {
                        std::cout << "Coordinates are " << coords.latitude << ", " << coords.longitude << std::endl;
                    }
                }
            } else if (token == "instruction") {
                int object = 0, command = 0, position = 0;
                if (ss >> object >> command >> position) {
                    msg::Message instruction = {
                        .type = msg::INSTRUCTIONS,
                        .content = {std::to_string(object), std::to_string(command), std::to_string(position)}};
                    instr_msg_queue.push(instruction);
                    std::cout << "Instruction added to queue: " << object << ", " << command << ", " << position
                              << std::endl;
                } else {
                    std::cout << "Invalid instruction" << std::endl;
                }
            } else if (token == "wifi") {
                std::string ssid;
                if (ss >> ssid) {
                    std::cout << "Enter the password for " << ssid << ": ";
                    std::cout.flush();
                    std::string password = "";
                    int rc = input(password, TIMEOUT, true);
                    if (rc >= 0) {
                        commbridge->send(msg::wifi(ssid, password));
                        // TODO: do we need to wait for response?
                        std::fill(password.begin(), password.end(), '*');
                        std::cout << "Sent wifi credentials: " << ssid << " " << password << std::endl;
                        if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                    }
                }
            } else if (token == "server") {
                std::string address;
                int port = 0;
                if (ss >> address) {
                    if (!(ss >> port)) { std::cout << "No port specified" << std::endl; }
                    commbridge->send(msg::server(address, port));
                    // TODO: do we need to wait for response?
                    std::cout << "Sent server details: " << address << " " << port << std::endl;
                    if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                } else {
                    std::cout << "No address specified" << std::endl;
                }
            } else if (token == "token") {
                std::string token;
                if (ss >> token) {
                    commbridge->send(msg::api(token));
                    // TODO: do we need to wait for response?
                    std::cout << "Sent api token: " << token << std::endl;
                    if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                } else {
                    std::cout << "No api token specified" << std::endl;
                }
            }
#ifdef ENABLE_DEBUG
            else if (token == "debug_command") {
                int year = 0;
                int month = 0, day = 0, hour = 0, min = 0;
                double alt = 0.0, azi = 0.0;
                if (ss >> year >> month >> day >> hour >> min >> alt >> azi) {
                    Command command = {
                        .coords = {alt * M_PI / 180.0, azi * M_PI / 180.0},
                        .time = {.year = (int16_t)year,
                                 .month = (int8_t)month,
                                 .day = (int8_t)day,
                                 .hour = (int8_t)hour,
                                 .min = (int8_t)min,
                                 .sec = 0},
                    };

                    commands.push_back(command);
                    std::cout << "Command added to queue: " << year << ", " << month << ", " << day << ", " << hour
                              << ", " << min << ", " << alt << ", " << azi << std::endl;
                } else {
                    std::cout << "Invalid command" << std::endl;
                }
            } else if (token == "debug_picture") {
                int image_id = 0;
                if (ss >> image_id) {
                    commbridge->send(msg::picture(image_id));
                    // TODO: do we need to wait for response?
                    std::cout << "Sent picture request: " << image_id << std::endl;
                    if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                } else {
                    std::cout << "No image id specified" << std::endl;
                }
            } else if (token == "debug_rec_msg") {
                std::string msg_str;
                msg::Message msg;
                if (ss >> msg_str) {
                    if (size_t pos = msg_str.find(';'); pos != std::string::npos) { msg_str.erase(pos); }
                    if (msg::convert_to_message(msg_str, msg) == 0) {
                        msg_queue->push(msg);
                        std::cout << "Message added to receive queue: " << msg_str << std::endl;
                    } else {
                        std::cout << "Invalid message (" << rc << "): " << msg_str << std::endl;
                    }
                } else {
                    std::cout << "No message specified" << std::endl;
                }
            } else if (token == "debug_send_msg") {
                msg::Message msg;
                std::string type_str;
                if (ss >> type_str) {
                    if (msg.type = msg::verify_message_type(type_str); msg.type != msg::MessageType::UNASSIGNED) {
                        std::vector<std::string> content;
                        std::string content_str;
                        while (ss >> content_str) {
                            content.push_back(content_str);
                        }

                        if (content.size() > 0) {
                            msg.content = content;
                            commbridge->send(msg);
                            // TODO: do we need to wait for response?
                            std::cout << "Sent message with type " << type_str << std::endl;
                            if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                        } else {
                            std::cout << "No content specified" << std::endl;
                        }
                    } else {
                        std::cout << "Invalid message type" << std::endl;
                    }
                }
            } else if (token == "debug_trace") {
                int planet;
                if (ss >> planet) {
                    state = TRACE;
                    trace_object = {(Planets)planet};
                }
            }
#endif
            else {
                std::cout << "Invalid command: \"" << token << "\"" << std::endl;
            }
        }

        input_buffer.clear();
    }
}

int Controller::input(std::string &buffer, uint32_t timeout, bool hidden) {
    char last_c = '\0';
    bool newline = false;
    int count = 0;

    while (!newline) {
        int ch = stdio_getchar_timeout_us(timeout);
        if (ch == PICO_ERROR_TIMEOUT) {
            count = -1;
            newline = true;
            std::cout << std::endl << "--Timeout--" << std::endl;
        } else {
            char c = static_cast<char>(ch);
            if ((c == '\r' || c == '\n') && last_c != '\r' && last_c != '\n') {
                last_c = c;
                newline = true;
                std::cout << std::endl;
            } else if (c == '\b') {
                if (buffer.size() > 0) {
                    buffer.pop_back();
                    std::cout << '\b' << ' ' << '\b';
                    std::cout.flush();
                }
            } else if (std::isprint(c)) {
                last_c = c;
                buffer += c;
                count++;
                if (!hidden) {
                    std::cout << c;
                } else {
                    std::cout << '*';
                }
                std::cout.flush();
            }
        }
    }

    return count;
}

void Controller::wait_for_event(absolute_time_t abs_time, int max_sleep_time) {
    while (!clock->is_alarm_ringing() && !input_detected() &&
           absolute_time_diff_us(abs_time, get_absolute_time()) < max_sleep_time) {
        sleep_ms(1000); // TODO: TBD
    }
}

bool Controller::input_detected() {
    const uint32_t INITIAL_TIMEOUT = 5000;
    if (stdio_getchar_timeout_us(INITIAL_TIMEOUT) != PICO_ERROR_TIMEOUT) {
        input_received = true;
        return true;
    }
    return false;
}

void Controller::trace() {
    if (!trace_started) {
        DEBUG("Starting trace for planet:");
        trace_object.print_planet();
        trace_object.set_observer_coordinates(gps->get_coordinates());
        Command start = trace_object.get_interest_point_command(ABOVE, clock->get_datetime());
        Command stop = trace_object.get_interest_point_command(BELOW, start.time);
        int difference = calculate_hour_difference(start.time, stop.time);
        DEBUG("Trace length:", difference);
        if (difference <= 0) {
            DEBUG("Trace can't start");
            state = SLEEP;
            return;
        }
        trace_object.start_trace(start.time, difference);
        trace_started = true;
        state = MOTOR_CALIBRATE;
        return;
    }
    if (mctrl->isRunning()) {
        state = COMM_READ;
        return;
    }
    trace_command = trace_object.next_trace();
    if (trace_command.time.year == -1) {
        mctrl->off();
        trace_started = false;
        state = COMM_READ;
        return;
    }
    mctrl->turn_to_coordinates(trace_command.coords);
    DEBUG("Trace coordinates altitude:", trace_command.coords.altitude * 180.0 / M_PI,
          "azimuth:", trace_command.coords.azimuth * 180.0 / M_PI);
    DEBUG("Trace Date day:", (int)trace_command.time.day, "hour", (int)trace_command.time.hour, "min",
          (int)trace_command.time.min);
}

bool Controller::config_wait_for_response() {
    std::cout << "Waiting for response from ESP..." << std::endl << "Press any key to skip" << std::endl;
    uint64_t time = time_us_64();
    while (time_us_64() - time < 60000000) {
        if (input_detected()) { return false; }
        commbridge->read_and_parse(1000, true);
        if (msg_queue->size() > 0) {
            comm_process();
            return true;
        }
    }
    return false;
}
