#include "controller.hpp"

#include "debug.hpp"
#include "message.hpp"
#include "sleep_functions.hpp"

#include <algorithm>
#include <cctype>
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
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), mctrl(motor_controller), msg_queue(msg_queue) {
    state = COMM_READ;
}

void Controller::run() {
    if (config_mode()) { DEBUG("Exited config mode"); }
    if (!initialized) {
        DEBUG("Not yet initialized");
        if (init()) {
            initialized = true;
            gps->set_mode(GPS::Mode::STANDBY);
            DEBUG("Initialized");
        } else {
            DEBUG("Failed to initialize");
            return;
        }
    }

    int image_id = 1; // TODO: ^
    DEBUG("Starting main loop");
    while (true) {
        if (config_mode()) { DEBUG("Exited config mode"); }
        DEBUG("State: ", state);
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
                mctrl->turn_to_coordinates(commands.front().coords);
                check_motor = true;
            case MOTOR_WAIT:
                if (mctrl->isRunning())
                    state = COMM_READ;
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
            case SLEEP:
                // DEBUG("Sleeping");
                if (double_check) {
                    state = COMM_READ;
                } else {
                    sleep_ms(1000);
                    // sleep_for(10, 9);
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
    int attempts = 0;

    if (!gps->get_coordinates().status) gps->set_mode(GPS::Mode::FULL_ON);
    if (!clock->is_synced()) commbridge->send(msg::datetime_request());
    // compass->calibrate();
    //  TODO: use compass to correct azimuth of commands
    while (!result && attempts < 9) {
        if (commbridge->read_and_parse(1000, true) > 0) { comm_process(); }
        if (!gps->get_coordinates().status) gps->locate_position(2);

        //result = true; // TODO: Remove
        if (gps->get_coordinates().status && clock->is_synced()) {
            result = true;
        } else {
            DEBUG("GPS status:", gps->get_coordinates().status);
            DEBUG("Clock synced:", clock->is_synced());
        }
        attempts++;
    }

    return result;
}

void Controller::comm_process() {
    DEBUG("Processing messages");
    while (msg_queue->size() > 0) {
        DEBUG(msg_queue->size());
        msg::Message msg = msg_queue->front();

        switch (msg.type) {
            case msg::RESPONSE: // Received response ACK/NACK from ESP
                DEBUG("Received response");
                if (msg.content[0] == "1") {
                    if (last_sent == msg::PICTURE) { state = MOTOR_OFF; }
                } else {
                    if (last_sent == msg::PICTURE) { state = COMM_READ; }
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
    msg::Message instr = instr_msg_queue.front();
    bool error = false;

    if (instr.content.size() == 3 && instr.type == msg::INSTRUCTIONS) {
        int planet_num;
        Planets planet;
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
            if (position < 1 || position > 3) error = true;
        } else
            error = true;

        if (!error) {
            Celestial celestial(planet);
            celestial.set_observer_coordinates(gps->get_coordinates());
            commands.push_back(celestial.get_interest_point_command((Interest_point)position, clock->get_datetime()));
            // TODO: correct azimuth
            std::sort(commands.begin(), commands.end(), compare_time);
            DEBUG("next command: ", (int)commands.front().time.year, (int)commands.front().time.month,
                  (int)commands.front().time.day, (int)commands.front().time.hour, (int)commands.front().time.min);
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

bool Controller::config_mode() {
    const int64_t INITIAL_TIMEOUT = 5000;
    const int64_t TIMEOUT = 60000000;

    if (stdio_getchar_timeout_us(INITIAL_TIMEOUT) != PICO_ERROR_TIMEOUT) {
        DEBUG("Stdio input detected. Entering config mode...");
        std::string input_buffer = "";
        bool exit = false;

        std::cout << "Stargazer config mode - type \"help\" for available commands";
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
                              << "debug_rec_msg <message> - add a message to the receive queue" << std::endl
                              << "debug_send_msg <message_type> <message_content_1> ... - send a message to the ESP"
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
                        std::cout << "Time set to " << now.year << "-" << +now.month << "-" << +now.day << " "
                                  << +now.hour << ":" << +now.min << std::endl;
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
                        }
                    }
                } else if (token == "server") {
                    std::string address;
                    int port = 0;
                    if (ss >> address) {
                        if (!(ss >> port)) { std::cout << "No port specified" << std::endl; }
                        commbridge->send(msg::server(address, port));
                        std::cout << "Sent server details: " << address << " " << port << std::endl;
                    } else {
                        std::cout << "No address specified" << std::endl;
                    }
                } else if (token == "token") {
                    std::string token;
                    if (ss >> token) {
                        commbridge->send(msg::api(token));
                        std::cout << "Sent api token: " << token << std::endl;
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
                    } else {
                        std::cout << "No image id specified" << std::endl;
                    }
                } else if (token == "debug_rec_msg") {
                    std::string msg_str;
                    msg::Message msg;
                    if (ss >> msg_str) {
                        if (size_t pos = msg_str.find(';'); pos != std::string::npos) {
                            msg_str.erase(pos);
                        }
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
                            } else {
                                std::cout << "No content specified" << std::endl;
                            }
                        } else {
                            std::cout << "Invalid message type" << std::endl;
                        }
                    }
                }
#endif
                else {
                    std::cout << "Invalid command: \"" << token << "\"" << std::endl;
                }
            }

            input_buffer.clear();
        }
        return true;
    } else {
        return false;
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
