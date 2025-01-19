#include "stepper-motor.hpp"
#include "stepper.pio.h"

StepperMotor::StepperMotor(const std::vector<uint> &stepper_pins, const std::vector<uint> &opto_fork_pins)
    : pins(stepper_pins), optoForkPins(opto_fork_pins), direction(true), pioInstance(nullptr), programOffset(0),
      stateMachine(0), speed(0), sequenceCounter(0), stepCounter(0), stepMax(6000),
      edgeSteps(0), stepMemory(0), stepperCalibrated(false), stepperCalibrating(false) {
        // need 4 pins
        if (pins.size() != 4) panic("Need 4 pins to operate stepper motor. number of pins got: %d", pins.size());
        // Three first stepper pins must be less than 6 apart
        if (pins[2] - pins [0] > 5) panic("Three first stepper pins must be less than 6 apart. They are %d apart", pins[2] - pins [0]);
      }

void StepperMotor::init(PIO pio, float rpm, bool clockwise) {
    pioInstance = pio;
    speed = rpm;
    direction = clockwise;
    pioInit();
    pins_init();
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

// this doesn't set the statemachine running
void StepperMotor::pioInit(void) {
    if (direction) {
        programOffset = pio_add_program(pioInstance, &stepper_clockwise_program);
    } else {
        programOffset = pio_add_program(pioInstance, &stepper_anticlockwise_program);
    }
    pio_sm_config conf = stepper_clockwise_program_get_default_config(programOffset);
    sm_config_set_clkdiv(&conf, calculateClkDiv(speed));
    stateMachine = pio_claim_unused_sm(pioInstance, true);
    pio_sm_init(pioInstance, stateMachine, programOffset, &conf);
}

float StepperMotor::calculateClkDiv(float rpm) const {
    if (rpm > RPM_MAX) rpm = RPM_MAX;
    //    if (rpm < RPM_MIN) rpm = RPM_MIN;
    return (SYS_CLK_KHZ * 1000) / (16000 / (((1 / rpm) * 60 * 1000) / 4096));
}

// needs to be called after initializing PIO and statemachine
// The pins have to be ascending in order
void StepperMotor::pins_init() {
    // TODO: maybe make an optofork class that handles optofork related things
    if (optoForkPins.size() > 0) {
        for (uint optoForkPin : optoForkPins) {
            gpio_set_dir(optoForkPin, GPIO_IN);
            gpio_set_function(optoForkPin, GPIO_FUNC_SIO);
            gpio_pull_up(optoForkPin);
        }
    }
    // mask is needed to set pin directions in the state machine
    uint32_t pin_mask = 0x0;
    for (uint pin : pins) {
        pin_mask |= 1 << pin;
        pio_gpio_init(pioInstance, pin);
    }
    pio_sm_set_pindirs_with_mask(pioInstance, stateMachine, pin_mask, pin_mask);
    // Configure the set pins (first 3 pins) and the sideset pin (last pin)
    pio_sm_set_set_pins(pioInstance, stateMachine, pins[0], pins[2] - pins[0] + 1);
    pio_sm_set_sideset_pins(pioInstance, stateMachine, pins[3]);
    // this changes the statemachines bytecode to match the pins used.
    morph_pio_pin_definitions();
}

// this needs to be ran everytime the program changes!! (changing directions etc)
// changes the statemachines bytecode to match the pin layout
void StepperMotor::morph_pio_pin_definitions(void) {
    // TODO: make a cool equation to reduce code duplication
    uint pin1 = 1;
    uint pin2 = 1 << (pins[1] - pins[0]);
    uint pin3 = 1 << (pins[2] - pins[0]);
    std::vector<uint> instructions;
    instructions.reserve(8);
    // pin1
    instructions.emplace_back(pio_encode_set(pio_pins, pin1) | pio_encode_delay(7) | pio_encode_sideset(2, 0b10));
    // pin1 and 2
    instructions.emplace_back(pio_encode_set(pio_pins, pin1 | pin2) | pio_encode_delay(7) | pio_encode_sideset(2, 0b10));
    // pin2
    instructions.emplace_back(pio_encode_set(pio_pins, pin2) | pio_encode_delay(7) | pio_encode_sideset(2, 0b10));
    // pin2 and pin 3
    instructions.emplace_back(pio_encode_set(pio_pins, pin2 | pin3) | pio_encode_delay(7) | pio_encode_sideset(2, 0b10));
    // pin3
    instructions.emplace_back(pio_encode_set(pio_pins, pin3) | pio_encode_delay(7) | pio_encode_sideset(2, 0b10));
    // pin3 and pin4 (sideset)
    instructions.emplace_back(pio_encode_set(pio_pins, pin3) | pio_encode_delay(7) | pio_encode_sideset(2, 0b11));
    // no first 3 pins active pin4 active (sideset)
    instructions.emplace_back(pio_encode_set(pio_pins, 0) | pio_encode_delay(7) | pio_encode_sideset(2, 0b11));
    // pin1 and pin4 (sideset)
    instructions.emplace_back(pio_encode_set(pio_pins, pin1) | pio_encode_delay(7) | pio_encode_sideset(2, 0b11));
    if (!direction) {
        std::reverse(instructions.begin(), instructions.end());
    }
    for (int i=0; i<8; i++) {
        pioInstance->instr_mem[programOffset + stepper_clockwise_offset_loop + i*3] = instructions[i];
    }
}

void StepperMotor::turnSteps(uint16_t steps) {
    uint32_t word = ((programOffset + stepper_clockwise_offset_loop) << 16) | steps;
    pio_sm_put_blocking(pioInstance, stateMachine, word);

    sequenceCounter = (sequenceCounter + steps) % 8;
    int16_t stepsToAdd = direction ? steps : -steps;
    stepCounter = (stepCounter + stepsToAdd) % stepMax;
    stepMemory = (stepMemory << 16) | stepsToAdd;
}

void StepperMotor::turnOneRevolution() {
    turnSteps(stepMax);
}

void StepperMotor::setSpeed(float rpm) {
    pio_sm_set_enabled(pioInstance, stateMachine, false);
    speed = rpm;
    float div = calculateClkDiv(rpm);
    pio_sm_set_clkdiv(pioInstance, stateMachine, div);
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

void StepperMotor::stop() {
    pio_sm_set_enabled(pioInstance, stateMachine, false);
    uint fifoLevel = pio_sm_get_tx_fifo_level(pioInstance, stateMachine);

    int32_t stepsLeft = 0; // Placeholder for logic from stepper_read_steps_left
    stepCounter = (stepCounter - stepsLeft) % stepMax;

    for (int i = 0; i < fifoLevel; i++) {
        stepCounter = (stepCounter - (int16_t)(stepMemory & 0xffff)) % stepMax;
        stepMemory >>= 16;
    }

    pio_sm_clear_fifos(pioInstance, stateMachine);
    pio_sm_exec(pioInstance, stateMachine, pio_encode_jmp(0));
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

void StepperMotor::setDirection(bool clockwise) {
    if (clockwise == direction) return; // no need to do anything
    
    stop();
    pio_sm_set_enabled(pioInstance, stateMachine, false);
    direction = clockwise;
    pio_clear_instruction_memory(pioInstance);
    if (clockwise) {
        programOffset = pio_add_program(pioInstance, &stepper_clockwise_program);
    } else {
        programOffset = pio_add_program(pioInstance, &stepper_anticlockwise_program);
    }
    morph_pio_pin_definitions();
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

uint8_t StepperMotor::getCurrentStep() const {
    uint8_t pinsOnOff = gpio_get(pins[3]) << 3 | gpio_get(pins[2]) << 2 | gpio_get(pins[1]) << 1 | gpio_get(pins[0]);

    switch (pinsOnOff) {
        case 0x01: return 0;
        case 0x03: return 1;
        case 0x02: return 2;
        case 0x06: return 3;
        case 0x04: return 4;
        case 0x0C: return 5;
        case 0x08: return 6;
        case 0x09: return 7;
        default: return 0xFF; // Undefined state
    }
}

bool StepperMotor::isRunning() const {
    return !((pio_sm_get_pc(pioInstance, stateMachine) == 0) &&
             (pio_sm_get_tx_fifo_level(pioInstance, stateMachine) == 0));
}

bool StepperMotor::isCalibrated() const {
    return stepperCalibrated;
}

bool StepperMotor::isCalibrating() const {
    return stepperCalibrating;
}

uint16_t StepperMotor::getMaxSteps() const {
    return stepMax;
}

uint16_t StepperMotor::getEdgeSteps() const {
    return edgeSteps;
}

int16_t StepperMotor::getStepCount() const {
    return stepCounter;
}

bool StepperMotor::getDirection() const {
    return direction;
}
