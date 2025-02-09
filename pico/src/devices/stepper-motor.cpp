#include "stepper-motor.hpp"
#include "stepper.pio.h"
#include "structs.hpp"

static StepperMotor *stepper_horizontal;
static StepperMotor *stepper_vertical;
void raw_calibration_handler(void) {
    int horizontal_opto=-1;
    int vertical_opto=-1;
    if (stepper_horizontal) horizontal_opto = stepper_horizontal->optoForkPin;
    if (stepper_vertical) vertical_opto = stepper_vertical->optoForkPin;
    if (gpio_get_irq_event_mask(horizontal_opto) & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(horizontal_opto, GPIO_IRQ_EDGE_RISE);
        stepper_horizontal->calibration_handler(true);
    } else if (gpio_get_irq_event_mask(horizontal_opto) & GPIO_IRQ_EDGE_FALL) {
        gpio_acknowledge_irq(horizontal_opto, GPIO_IRQ_EDGE_FALL);
        stepper_horizontal->calibration_handler(false);
    } else if (gpio_get_irq_event_mask(vertical_opto) & GPIO_IRQ_EDGE_RISE) {
        gpio_acknowledge_irq(vertical_opto, GPIO_IRQ_EDGE_RISE);
        stepper_vertical->calibration_handler(true);
    } else if (gpio_get_irq_event_mask(vertical_opto) & GPIO_IRQ_EDGE_FALL) {
        gpio_acknowledge_irq(vertical_opto, GPIO_IRQ_EDGE_FALL);
        stepper_vertical->calibration_handler(false);
    } else {
    panic("Interrupt called without initializing any stepper motors (this shouldn't be possible)");
    }
}

int modulo(int x, int y) {
    int rem = x % y;
    return rem + ((rem < 0) ? y : 0);
}

StepperMotor::StepperMotor(const std::vector<uint> &stepper_pins, int optoforkpin, Axis axis)
    : pins(stepper_pins), optoForkPin(optoforkpin), direction(true), pioInstance(nullptr), programOffset(0),
      stateMachine(0), speed(0), sequenceCounter(0), stepCounter(0), stepMax(4097),
      edgeSteps(0), stepMemory(0), stepperCalibrated(false), stepperCalibrating(false), axis(axis) {
        // need 4 pins
        if (pins.size() != 4) panic("Need 4 pins to operate stepper motor. number of pins got: %d", pins.size());
        // Three first stepper pins must be less than 6 apart
        if (pins[2] - pins [0] > 5) panic("Three first stepper pins must be less than 6 apart. They are %d apart", pins[2] - pins [0]);
        set_calibration_pointer(axis);
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
    if (rpm < RPM_MIN) rpm = RPM_MIN;
    return (SYS_CLK_KHZ * 1000) / (16000 / (((1 / rpm) * 60 * 1000) / 4096));
}

// needs to be called after initializing PIO and statemachine
// The pins have to be ascending in order
void StepperMotor::pins_init() {
    // TODO: maybe make an optofork class that handles optofork related things
    if (optoForkPin) {
        gpio_set_dir(optoForkPin, GPIO_IN);
        gpio_set_function(optoForkPin, GPIO_FUNC_SIO);
        gpio_pull_up(optoForkPin);
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

void StepperMotor::set_calibration_pointer(Axis axis) {
    if (axis == HORIZONTAL) stepper_horizontal = this;
    else if (axis == VERTICAL) stepper_vertical = this;
    this->axis = axis;
}

void StepperMotor::turnSteps(uint16_t steps) {
    uint32_t word = ((programOffset + stepper_clockwise_offset_loop + 3 * sequenceCounter) << 16) | (steps);
    pio_sm_put_blocking(pioInstance, stateMachine, word);

    sequenceCounter = modulo(sequenceCounter + steps, 8);
    int16_t stepsToAdd = direction ? steps : -steps;
    stepCounter = modulo(stepCounter + stepsToAdd, stepMax);
    stepMemory = (stepMemory << 16) | stepsToAdd;
}


// this will stop the motor and turn to an angle instantly
void StepperMotor::turn_to(double radians) {
    stop();
    double current = get_position();
    double distance = radians - current;
    if (distance < 0)
        setDirection(ANTICLOCKWISE);
    else
        setDirection(CLOCKWISE);
    turnSteps(radians_to_steps(fabs(distance)));
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

    sequenceCounter = modulo(getCurrentStep() + 1, 8);
    if (direction == ANTICLOCKWISE) {
        sequenceCounter = modulo(7 - (sequenceCounter - 1) + 1, 8);
    }
    uint fifoLevel = pio_sm_get_tx_fifo_level(pioInstance, stateMachine);
    int32_t stepsLeft = read_steps_left();
    stepCounter = modulo(stepCounter - stepsLeft, stepMax);

    for (int i = 0; i < fifoLevel; i++) {
        stepCounter = modulo(stepCounter - (int16_t)(stepMemory & 0xffff), stepMax);
        stepMemory >>= 16;
    }

    pio_sm_clear_fifos(pioInstance, stateMachine);
    pio_sm_exec(pioInstance, stateMachine, pio_encode_jmp(0));
    pio_sm_set_enabled(pioInstance, stateMachine, true);
}

int StepperMotor::read_steps_left(void) {
    uint8_t pc = pio_sm_get_pc(pioInstance, stateMachine); // Get the current program counter
    uint8_t loop_offset = programOffset + stepper_clockwise_offset_loop;
    uint32_t steps_left = 0;
    if (pc == 0 || pc == 1) {
        // If the program counter is 0 or 1, the program hasn't pulled from the RX FIFO, so no steps executed
        return 0;
    } else if (pc == 2) {
        // If the program counter is 2, steps to take haven't been pushed to the X register yet
        pio_sm_exec(pioInstance, stateMachine, pio_encode_out(pio_x, 16)); // Push steps to take to the X register
    } else if ((pc >= loop_offset)) {
        // If the program counter is in the loop region
        if (((pc - loop_offset) % 3) == 0) {
            // Depending on the program counter position, it might require adding one more step
            if (((pc - loop_offset) / 3) == modulo(sequenceCounter + (direction ? 1 : -1), 8)) {
                steps_left++; // If the PC is on the SET instruction but hasn't decremented X yet
            }
        }
    }
    // Move the X register contents to the ISR, then push ISR contents to the RX FIFO
    pio_sm_exec(pioInstance, stateMachine, pio_encode_in(pio_x, 32));
    pio_sm_exec(pioInstance, stateMachine, pio_encode_push(false, false));
    // Read the RX FIFO to determine the number of executed steps
    steps_left += pio_sm_get(pioInstance, stateMachine);
    return direction ? steps_left : -steps_left;
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

double StepperMotor::get_position(void) {
    return ((double)stepCounter * 2.0 * M_PI) / (double)stepMax;
}

int StepperMotor::radians_to_steps(double radians) {
    return (int)round((radians * (double)stepMax) / (2.0 * M_PI));
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

void StepperMotor::calibrate(void) {
    if (isCalibrating()) return;
    if (!optoForkPin) return;
    if (axis == UNDEFINED) return;
    setSpeed(2);
    stepperCalibrating = true;
    stepperCalibrated = false;

    gpio_add_raw_irq_handler_with_order_priority(optoForkPin, raw_calibration_handler, PICO_HIGHEST_IRQ_PRIORITY);
    gpio_set_irq_enabled(optoForkPin, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    if (!irq_is_enabled(IO_IRQ_BANK0)) irq_set_enabled(IO_IRQ_BANK0, true);

    turnSteps(6000);
}

void StepperMotor::calibration_handler(bool rise) {
    if (rise) {
        if (!isCalibrated()) {
            stop();
            stepCounter = 0;
            stepperCalibrated = true;
        }
    }
}
