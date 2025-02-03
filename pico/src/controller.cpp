#include "controller.hpp"

#include "debug.hpp"

Controller::Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
                       std::shared_ptr<CommBridge> commbridge, std::shared_ptr<StepperMotor> motor_horizontal,
                       std::shared_ptr<StepperMotor> motor_vertical,
                       std::shared_ptr<std::queue<msg::Message>> msg_queue)
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), motor_horizontal(motor_horizontal),
      motor_vertical(motor_vertical), msg_queue(msg_queue) {
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
    int object_id = 1; // TODO: remove this
    int image_id = 1;  // TODO: ^

    DEBUG("Starting main loop");
    while (true) {
        DEBUG("State: ", state);
        switch (state) {

            case COMM_READ:
                if (commbridge->read_and_parse(1000, true) > 0) {
                    state = COMM_PROCESS;
                } else {
                    state = INSTR_PROCESS;
                    break;
                }
            case COMM_PROCESS:
                comm_process();
            case INSTR_PROCESS:
                instr_process();
                break;
            case MOTOR_CONTROL:
                // TODO: Control motors to adjust the camera in the direction of the object
                break;
            case CAMERA_EXECUTE:
                commbridge->send(msg::picture(object_id, image_id));
                break;
            case SLEEP: // TODO: Go here after full round of nothing to do?
                DEBUG("Sleeping");
                sleep_ms(1000);
                // TODO: Sleeping
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

                }
                break;
            case msg::DATETIME:
                DEBUG("Received datetime");
                clock->update(msg.content[0]);
                break;
            case msg::ESP_INIT: // Send ACK response back to ESP
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
    }
}

void Controller::instr_process() {
    DEBUG("Processing instructions");
    if (instr_msg_queue.size() > 0) {
        // TODO: Process instructions
        msg::Message instr = instr_msg_queue.front();
        bool add = true;
        if (instr.content.size() == 3 && instr.type == msg::INSTRUCTIONS) {
            int planet_num;
            Planets planet;
            if (str_to_int(instr.content[0], planet_num)) {
                if (planet_num >= 1 && planet_num <= 9) planet = planet_num;
                else add = false;
            } else add = false;
            int id;
            if (!str_to_int(instr.content[1], id)) add = false;
            int position;
            if (str_to_int(instr.content[2], position)) {
                if (position < 1 || position > 3) add = false;
            } else add = false;

            if (add) {
                Celestial celestial(planet);
                commands.push_back()
            }
        }
        instr_msg_queue.pop();
    }
}

void Controller::motor_control() {
    DEBUG("Controlling motors");
    // TODO: Control motors to adjust the camera in the direction of the object
}
