#include "stepper-motor.hpp"
#include "stepper.pio.h"

StepperMotor::StepperMotor()
    : optoForkPin(0), direction(true), pioInstance(nullptr), programOffset(0),
      stateMachine(0), speed(0), sequenceCounter(0), stepCounter(0), stepMax(6000),
      edgeSteps(0), stepMemory(0), stepperCalibrated(false), stepperCalibrating(false) {
    for (int i = 0; i < NPINS; i++) {
        pins[i] = 0;
    }
}

void StepperMotor::init(PIO pio, const uint *stepperPins, uint optoForkPin, float rpm, bool clockwise) {
    pioInstance = pio;
    this->optoForkPin = optoForkPin;

    if (optoForkPin) {
        gpio_set_dir(optoForkPin, GPIO_IN);
        gpio_set_function(optoForkPin, GPIO_FUNC_SIO);
        gpio_pull_up(optoForkPin);
    }

    for (int i = 0; i < NPINS; i++) {
        pins[i] = stepperPins[i];
    }

    direction = clockwise;
    stateMachine = pio_claim_unused_sm(pioInstance, true);

    if (clockwise) {
        programOffset = pio_add_program(pioInstance, &stepper_clockwise_program);
    } else {
        programOffset = pio_add_program(pioInstance, &stepper_anticlockwise_program);
    }

    speed = rpm;
    float div = calculateClkDiv(rpm);
    pioInit(div);
}

void StepperMotor::pioInit(float div) {
    const pio_program_t *program = direction ? &stepper_clockwise_program : &stepper_anticlockwise_program;

    pio_sm_config conf = pio_get_default_sm_config();
    sm_config_set_clkdiv(&conf, div);
    sm_config_set_out_pins(&conf, pins[0], NPINS);
    sm_config_set_set_pins(&conf, pins[0], NPINS);
    sm_config_set_sideset_pins(&conf, pins[3]);

    uint pinMask = 0;
    for (int i = 0; i < NPINS; i++) {
        pinMask |= 1 << pins[i];
        pio_gpio_init(pioInstance, pins[i]);
    }

    pio_sm_set_pindirs_with_mask(pioInstance, stateMachine, pinMask, pinMask);
    pio_sm_init(pioInstance, stateMachine, programOffset, &conf);
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

float StepperMotor::calculateClkDiv(float rpm) const {
    if (rpm > RPM_MAX) rpm = RPM_MAX;
    if (rpm < RPM_MIN) rpm = RPM_MIN;
    return (SYS_CLK_KHZ * 1000) / (16000 / (((1 / rpm) * 60 * 1000) / 4096));
}

void StepperMotor::turnSteps(uint16_t steps) {
    uint32_t word = ((programOffset + (direction ? stepper_clockwise_offset_loop : stepper_anticlockwise_offset_loop)) << 16) | steps;
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
    if (clockwise != direction) {
        stop();
        pio_sm_set_enabled(pioInstance, stateMachine, false);
        direction = clockwise;
        pio_clear_instruction_memory(pioInstance);

        if (clockwise) {
            programOffset = pio_add_program(pioInstance, &stepper_clockwise_program);
        } else {
            programOffset = pio_add_program(pioInstance, &stepper_anticlockwise_program);
        }

        pio_sm_set_enabled(pioInstance, stateMachine, true);
    }
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
