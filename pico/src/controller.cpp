#include "controller.hpp"

#include "debug.hpp"
#include "message.hpp"

Controller::Controller(std::shared_ptr<Clock> clock, std::shared_ptr<GPS> gps, std::shared_ptr<Compass> compass,
                       std::shared_ptr<CommBridge> commbridge, std::shared_ptr<std::queue<msg::Message>> msg_queue)
    : clock(clock), gps(gps), compass(compass), commbridge(commbridge), msg_queue(msg_queue) {}

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

    int object_id = 1; // TODO: From somewhere
    int image_id = 1;  // TODO: ^

    DEBUG("Starting main loop");
    while (true) {
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
                break;
            case INSTR_PROCESS:
                instr_process();
                break;
            case MOTOR_CONTROL:
                // TODO: Control motors to adjust the camera in the direction of the object
                break;
            case CAMERA_EXECUTE:
                commbridge->send(msg::picture(object_id, image_id));
                break;
            default:
                DEBUG("Unknown state: ", state);
                state = COMM_READ;
                break;
        }
    }
}

bool Controller::init() {
    bool result = false;
    int attempts = 0;

    gps->set_mode(GPS::Mode::FULL_ON);
    commbridge->send(msg::datetime_request());
    compass->calibrate();
    // Motor calibration
    while (!result && attempts < 9) {
        comm_process();
        gps->locate_position(3);
        if (gps->get_coordinates().status && clock->is_synced()) { result = true; }
        attempts++;
    }

    return result;
}

void Controller::comm_read() {}

void Controller::comm_process() {
    while (msg_queue->size() > 0) {
        msg::Message msg = msg_queue->front();

        switch (msg.type) {
            case msg::RESPONSE: // Received response ACK/NACK from ESP
                DEBUG("Received response");
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
                break;
            default:
                DEBUG("Unexpected message type: ", msg.type);
                break;
        }

        msg_queue->pop();
    }
}

void Controller::instr_process() {
    if (new_instr_queue.size() > 0) {
        // TODO: Process instructions
        msg::Message instr = new_instr_queue.front();
        new_instr_queue.pop();
    }
}

void Controller::motor_control() {
    // TODO: Control motors to adjust the camera in the direction of the object
}
