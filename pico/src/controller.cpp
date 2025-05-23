#include "controller.hpp"

#include "convert.hpp"
#include "date_utils.hpp"
#include "debug.hpp"
#include "message.hpp"

#include <algorithm>
#include <cctype>
#include <hardware/timer.h>
#include <pico/stdio.h>
#include <pico/time.h>
#include <pico/types.h>

/**
 * @brief Compares two commands based on their time.
 *
 * @param a First command.
 * @param b Second command.
 * @return bool True if a is less than b, False otherwise.
 */
bool compare_time(const Command &a, const Command &b) {
    if (a.time.year != b.time.year) return a.time.year < b.time.year;
    if (a.time.month != b.time.month) return a.time.month < b.time.month;
    if (a.time.day != b.time.day) return a.time.day < b.time.day;
    if (a.time.hour != b.time.hour) return a.time.hour < b.time.hour;
    if (a.time.min != b.time.min) return a.time.min < b.time.min;
    return a.time.sec < b.time.sec;
}

/**
 * @brief Constructor for the Controller class.
 *
 * @param clock Pointer to the Clock object.
 * @param gps Pointer to the GPS object.
 * @param compass Pointer to the Compass object.
 * @param commbridge Pointer to the CommBridge object.
 * @param motor_controller Pointer to the MotorControl object.
 * @param storage Pointer to the Storage object.
 * @param msg_queue Pointer to the message queue.
 */
Controller::Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
                       std::shared_ptr<CommBridge> commbridge, std::shared_ptr<MotorControl> motor_controller,
                       std::shared_ptr<Storage> storage, std::shared_ptr<std::queue<msg::Message>> msg_queue)
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), mctrl(motor_controller), storage(storage),
      msg_queue(msg_queue) {}

/**
 * @brief Main function for the Controller class.
 * @details Handles the main control flow of the Pico.
 */
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
            send(msg::device_status(true));
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
        sanitize_commands();

        switch (state) {
            case COMM_READ:
                double_check = false;
                commbridge->read_and_parse(1000, true);
            case COMM_SEND:
                send_process();
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
                else if (now_commands > 0)
                    state = MOTOR_CALIBRATE;
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
                motor_control();
                break;
            case MOTOR_WAIT:
                if (mctrl->isRunning())
                    state = COMM_READ;
                else {
                    state = COMM_READ;
                    check_motor = false;
                    send(msg::picture(current_command.id));
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
                    wait_for_event(get_absolute_time(), 100000); // 100ms
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

/**
 * @brief Sanitizes the commands queue.
 * @details Checks if the first command is too old and removes it if it is.
 */
void Controller::sanitize_commands() {
    if (commands.size() <= 0) return;
    if (now_commands > 0) return;
    std::sort(commands.begin(), commands.end(), compare_time);
    Command front = commands.front();
    if (calculate_sec_difference(front.time, clock->get_datetime()) > 1) {
        DEBUG("Command was too old, discarding");
        send(msg::cmd_status(front.id, -2, 0));
        commands.erase(commands.begin());
    } else {
        clock->add_alarm(front.time);
    }
}

/**
 * @brief Initializes the Pico.
 * @details Sets the GPS mode to FULL_ON, gets the GPS coordinates, and checks if the clock is synced.
 * @return bool True if initialization was successful, otherwise False.
 */
bool Controller::init() {
    DEBUG("Initializing");
    bool result = false;

#ifdef GPS_COORDS // For testing purposes
    gps->set_coordinates(60.258656, 24.843641);
#endif
    if (!gps->get_coordinates().status) {
        if (gps->get_mode() != GPS::Mode::FULL_ON) gps->set_mode(GPS::Mode::FULL_ON);
    }
    if (!clock->is_synced()) {
        if (commbridge->ready_to_send()) { commbridge->send(msg::datetime_request()); }
    }

    if (commbridge->read_and_parse(1000, true) > 0) { comm_process(); }
    if (!gps->get_coordinates().status) gps->locate_position(2);

    if (gps->get_coordinates().status && clock->is_synced()) { result = true; }
    DEBUG("GPS fix status:", gps->get_coordinates().status);
    DEBUG("Clock sync status:", clock->is_synced());

    return result;
}

/**
 * @brief Processes messages from the message queue.
 * @details Checks if a message is ready to be processed and tries to process it.
 */
void Controller::comm_process() {
    DEBUG("Processing messages");
    double_check = true;
    state = SLEEP;
    if (commbridge->ready_to_send() && waiting_for_response) {
        DEBUG("ESP didn't respond to message of type:", static_cast<int>(last_sent));
        send(msg::diagnostics(2, "ESP didn't respond to message"));
        waiting_for_response = false;
        if (last_sent == msg::PICTURE) { state = MOTOR_OFF; }
    }
    while (msg_queue->size() > 0) {
        DEBUG(msg_queue->size());
        msg::Message msg = msg_queue->front();
        DEBUG("Last sent is:", static_cast<int>(last_sent));
        waiting_for_response = false;
        switch (msg.type) {
            case msg::RESPONSE: // Received response ACK/NACK from ESP
                if (msg.content[0] == "1") {
                    DEBUG("Received ack");
                    if (last_sent == msg::PICTURE) { state = MOTOR_OFF; }
                } else {
                    DEBUG("Received nack");
                    if (last_sent == msg::PICTURE) { state = COMM_READ; }
                }
                break;
            case msg::DATETIME:
                DEBUG("Received datetime");
                clock->update(msg.content[0]);
                send(msg::response(true));
                break;
            case msg::DEVICE_STATUS: // Send ACK or DEVICE_STATUS response back to ESP
                DEBUG("Received ESP init");
                if (msg.content[0] == "1") {
                    esp_initialized = true;
                } else {
                    esp_initialized = false;
                }
                if (last_sent == msg::DEVICE_STATUS) {
                    send(msg::response(true));
                } else {
                    send(msg::device_status(true));
                }
                break;
            case msg::INSTRUCTIONS: // Store/Process instructions
                DEBUG("Received instructions");
                instr_msg_queue.push(msg);
                send(msg::response(true));
                break;
            default:
                DEBUG("Unexpected message type: ", msg.type);
                send(msg::response(false));
                break;
        }
        msg_queue->pop();
    }
}

/**
 * @brief Processes instructions from the instruction queue.
 * @details Checks if an instruction is ready to be processed and tries to process it.
 */
void Controller::instr_process() {
    DEBUG("Processing instructions");
    msg::Message instr = instr_msg_queue.front();
    instr_msg_queue.pop();
    double_check = true;
    bool error = false;
    state = SLEEP;
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

        int id;
        if (!str_to_int(instr.content[1], id)) error = true;

        int position;
        if (str_to_int(instr.content[2], position)) {
            if (position < 1 || position > 4) error = true;
        } else
            error = true;

        if (!error) {
            Celestial celestial(planet);
            Interest_point interest = static_cast<Interest_point>(position);
            celestial.set_observer_coordinates(gps->get_coordinates());
            Command command = celestial.get_interest_point_command(interest, clock->get_datetime());
            command.id = id;
            if (interest == NOW) {
                command.time = clock->get_datetime(); // we only add coordinates in the above function
                now_commands++;
            }
            if (command.coords.altitude < 0 || command.time.year < 2000) {
                DEBUG("Instruction not possible");
                DEBUG("command altitude:", command.coords.altitude * 180 / M_PI, "year:", command.time.year);
                send(msg::cmd_status(id, -2, 0));
                return;
            }

            send(msg::cmd_status(id, 2, datetime_to_epoch(command.time)));
            commands.push_back(command);
            std::sort(commands.begin(), commands.end(), compare_time);
            DEBUG("Next command: ", (int)commands.front().time.year, (int)commands.front().time.month,
                  (int)commands.front().time.day, (int)commands.front().time.hour, (int)commands.front().time.min);
            if (commands.size() > 0 && interest != NOW) clock->add_alarm(commands.front().time);
        } else {
            DEBUG("Error in instruction.");
            send(msg::cmd_status(id, -1, 0));
        }
    }
}

/**
 * @brief Enters config mode.
 * @details This function starts the config mode, which allows the user to send various commands to the Pico and ESP.
 * @note This function is blocking and only returns when the user exits the config mode or the timeout is reached.
 */
void Controller::config_mode() {
    const int64_t TIMEOUT = 60000000; // 60 seconds

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
            std::stringstream ss(input_buffer);
            std::string token;
            ss >> token;
            if (token == "help") {
                std::cout << "Available commands:" << std::endl
                          << "help - print this help message" << std::endl
                          << "exit - exit config mode" << std::endl
                          << "heading - set compass heading of the device" << std::endl
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
            } else if (token == "heading") {
                float heading = 0;
                if (ss >> heading) { mctrl->setHeading(heading); }
                std::cout << "Heading set to: " << heading << std::endl;
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
                    std::cout << "Sent server details: " << address << " " << port << std::endl;
                    if (!config_wait_for_response()) std::cout << "No response from ESP" << std::endl;
                } else {
                    std::cout << "No address specified" << std::endl;
                }
            } else if (token == "token") {
                std::string token;
                if (ss >> token) {
                    commbridge->send(msg::api(token));
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
                    trace_object = {static_cast<Planets>(planet)};

                    DEBUG("Trace starting after exiting config mode. Trace object:");
                    trace_object.print_planet();
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

/**
 * @brief Get input from the user.
 *
 * @param buffer The buffer to store the input.
 * @param timeout The timeout in microseconds.
 * @param hidden If true, the input is hidden.
 * @return int The number of characters read.
 */
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

/**
 * @brief Wait for an event to occur.
 *
 * @param abs_time The current absolute time.
 * @param max_sleep_time The maximum sleep time in microseconds.
 */
void Controller::wait_for_event(absolute_time_t abs_time, int max_sleep_time) {
    while (!clock->is_alarm_ringing() && !input_detected() &&
           absolute_time_diff_us(abs_time, get_absolute_time()) < max_sleep_time) {
        sleep_ms(50);
    }
}

/**
 * @brief Check stdio for input.
 *
 * @return bool True if input has been detected. False otherwise.
 */
bool Controller::input_detected() {
    const uint32_t INITIAL_TIMEOUT = 5000;
    if (stdio_getchar_timeout_us(INITIAL_TIMEOUT) != PICO_ERROR_TIMEOUT) {
        input_received = true;
        return true;
    }
    return false;
}

/**
 * @brief Enter trace mode.
 * @details This function starts the trace mode in which the Pico traces the orbit of a celestial body.
 * @note Function only works properly if trace_object is set.
 */
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
        trace_pause = true;
        state = MOTOR_CALIBRATE;
        return;
    }
    if (mctrl->isRunning()) {
        state = COMM_READ;
        return;
    } else if (trace_pause) {
        trace_time = time_us_64();
        trace_pause = false;
        state = COMM_READ;
        return;
    } else {
        uint64_t current_time = time_us_64();
        if (current_time - trace_time < 1000000) { // 1 second
            state = COMM_READ;
            return;
        }
    }
    trace_command = trace_object.next_trace();
    if (trace_command.time.year == -1) {
        DEBUG("Trace ended.");
        mctrl->off();
        trace_started = false;
        state = COMM_READ;
        return;
    }
    mctrl->turn_to_coordinates(trace_command.coords);
    trace_pause = true;
    DEBUG("Trace coordinates altitude:", trace_command.coords.altitude * 180.0 / M_PI,
          "azimuth:", trace_command.coords.azimuth * 180.0 / M_PI);
    DEBUG("Trace Date day:", (int)trace_command.time.day, "hour", (int)trace_command.time.hour, "min",
          (int)trace_command.time.min);
}

/**
 * @brief Wait for a response from the ESP.
 *
 * @return bool True if a response has been received. False otherwise.
 */
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

/**
 * @brief Control the motors.
 * @details This function moves the motors to the positions detailed in the command that is next in the queue.
 */
void Controller::motor_control() {
    if (now_commands > 0) now_commands--;
    state = SLEEP;
    if (commands.size() > 0) {
        int sec_difference = calculate_sec_difference(commands.front().time, clock->get_datetime());
        if (sec_difference < -(60 * 5)) {
            clock->add_alarm(commands.front().time);
            mctrl->off();
            state = SLEEP;
            return;
        } else if (sec_difference > (60 * 5)) {
            DEBUG("Time difference of command and current time was too large (>5 minutes).");
            send(msg::cmd_status(current_command.id, -3, datetime_to_epoch(clock->get_datetime())));
            commands.front().time = clock->get_datetime();
            mctrl->off();
            state = COMM_READ;
            return;
        } else {
            current_command = commands.front();
            commands.erase(commands.begin());
            DEBUG("turning to altitude:", current_command.coords.altitude * 180 / M_PI,
                  "azimuth:", current_command.coords.azimuth * 180 / M_PI);
            mctrl->turn_to_coordinates(current_command.coords);
            check_motor = true;
            state = MOTOR_WAIT;
        }
    } else {
        DEBUG("Tried to initiate picture taking with empty command vector.");
        send(msg::diagnostics(2, "Device tried to take picture with no command"));
        mctrl->off();
        state = COMM_READ;
    }
}

/**
 * @brief Send a message to the ESP.
 * @details This function checks if the message is a response and sends it directly to the ESP. Otherwise, it adds the
 * message to the send message queue.
 * @param mesg The message to be sent.
 */
void Controller::send(const msg::Message mesg) {
    if (mesg.type == msg::RESPONSE) {
        commbridge->send(mesg);
        return;
    }
    send_msg_queue.push(mesg);
}

/**
 * @brief Process the send message queue.
 * @details This function checks if there are any messages in the send message queue and sends them to the ESP
 * unless the Pico is waiting for a response from the ESP.
 */
void Controller::send_process() {
    if (send_msg_queue.size() <= 0) return;
    if (waiting_for_response) return;
    commbridge->send(send_msg_queue.front());
    last_sent = send_msg_queue.front().type;
    send_msg_queue.pop();
}
