#include "controller.hpp"

#include "debug.hpp"

Controller::Controller(std::shared_ptr<PicoUart> comm_uart, std::shared_ptr<PicoUart> gps_uart) {}

void Controller::run() {
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
            case INSTR_EXECUTE:
                instr_execute();
                break;
            default:
                DEBUG("Unknown state: ", state);
                state = COMM_READ;
                break;
        }
    }
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
                break;
            case msg::INSTRUCTIONS:
                DEBUG("Received instructions");
                break;
            case msg::PICTURE: // Pico should not receive these
                DEBUG("Received picture msg for some reason");
                break;
            case msg::DIAGNOSTICS: // Pico should not receive these
                DEBUG("Received diagnostics msg for some reason");
                break;
            default:
                DEBUG("Unknown message type: ", msg.type);
                break;
        }

        msg_queue->pop();
    }
}

void Controller::instr_process() {}

void Controller::instr_execute() {}
