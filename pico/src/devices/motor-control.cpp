#include "motor-control.hpp"

#define NATURAL_SPEED 3


// these are used for calibration
static MotorControl *motorcontrol;

MotorControl::MotorControl(std::shared_ptr<StepperMotor> horizontal, std::shared_ptr<StepperMotor> vertical,
                           int optopin_horizontal, int optopin_vertical)
    : motor_horizontal(horizontal), motor_vertical(vertical), opto_horizontal(optopin_horizontal),
      opto_vertical(optopin_vertical), horizontal_calibrated(false), vertical_calibrated(false),
      horizontal_calibrating(false), vertical_calibrating(false) {
    init_optoforks();
    motorcontrol = this;
    motor_horizontal->init(pio0, 5, CLOCKWISE);
    motor_vertical->init(pio1, 5, CLOCKWISE);
}

bool MotorControl::turn_to_coordinates(azimuthal_coordinates coords) {
    if (coords.altitude < MIN_ANGLE ||coords.altitude > MAX_ANGLE) {
        DEBUG("Altitude below horizon, can't turn the motor");
        return false;
    }
    if (coords.azimuth > MAX_ANGLE) {
        coords.azimuth -= MAX_ANGLE;
        if (coords.altitude < (M_PI / 2)) coords.altitude += M_PI - 2 * coords.altitude;
        else coords.altitude -= M_PI + 2 * coords.altitude;
    }
    double ratio = fabs(motor_horizontal->get_position() - coords.azimuth) / fabs(motor_vertical->get_position() - coords.altitude);
    double horizontal_speed = NATURAL_SPEED * ratio;
    motor_vertical->setSpeed(NATURAL_SPEED);
    motor_horizontal->setSpeed(horizontal_speed);
    motor_horizontal->turn_to(coords.azimuth);
    motor_vertical->turn_to(coords.altitude);
    std::cout <<"ratio " << ratio << " spe " << horizontal_speed << std::endl;
    return true;
}

void MotorControl::off(void) {
    horizontal_calibrated = false;
    vertical_calibrated = false;
}

//// PRIVATE ////

void MotorControl::init_optoforks(void) {
    if (opto_horizontal) {
        gpio_set_dir(opto_horizontal, GPIO_IN);
        gpio_set_function(opto_horizontal, GPIO_FUNC_SIO);
        gpio_pull_up(opto_horizontal);
    }
    if (opto_vertical) {
        gpio_set_dir(opto_vertical, GPIO_IN);
        gpio_set_function(opto_vertical, GPIO_FUNC_SIO);
        gpio_pull_up(opto_vertical);
    }
}

bool MotorControl::isCalibrated(void) const {
    return horizontal_calibrated && vertical_calibrated;
}

bool MotorControl::isCalibrating(void) const {
    return horizontal_calibrating || vertical_calibrating;
}

bool MotorControl::isRunning(void) const {
    return motor_horizontal->isRunning() || motor_vertical->isRunning();
}

//// CALIBRATION ////

void raw_calibration_handler(void) {
    if (!motorcontrol) {
        DEBUG("Motor control raw calibration called without initializing");
        return;
    }
    int horizontal_opto = motorcontrol->opto_horizontal;
    int vertical_opto = motorcontrol->opto_vertical;
    if (horizontal_opto) {
        if (gpio_get_irq_event_mask(horizontal_opto) & GPIO_IRQ_EDGE_RISE) {
            gpio_acknowledge_irq(horizontal_opto, GPIO_IRQ_EDGE_RISE);
            motorcontrol->calibration_handler(HORIZONTAL, true);
        } else if (gpio_get_irq_event_mask(horizontal_opto) & GPIO_IRQ_EDGE_FALL) {
            gpio_acknowledge_irq(horizontal_opto, GPIO_IRQ_EDGE_FALL);
            motorcontrol->calibration_handler(HORIZONTAL, false);
        }
    }
    if (vertical_opto) {
        if (gpio_get_irq_event_mask(vertical_opto) & GPIO_IRQ_EDGE_RISE) {
            gpio_acknowledge_irq(vertical_opto, GPIO_IRQ_EDGE_RISE);
            motorcontrol->calibration_handler(VERTICAL, true);
        } else if (gpio_get_irq_event_mask(vertical_opto) & GPIO_IRQ_EDGE_FALL) {
            gpio_acknowledge_irq(vertical_opto, GPIO_IRQ_EDGE_FALL);
            motorcontrol->calibration_handler(VERTICAL, false);
        }
    }
    
}


void MotorControl::calibrate(void) {
    if (isCalibrating()) return;
    if (!opto_horizontal) return;
    if (!opto_vertical) return;
    motor_horizontal->stop();
    motor_vertical->stop();
    motor_horizontal->setSpeed(2);
    motor_vertical->setSpeed(2);
    motor_horizontal->setDirection(ANTICLOCKWISE);
    motor_vertical->setDirection(ANTICLOCKWISE);
    horizontal_calibrated = false;
    vertical_calibrated = false;
    horizontal_calibrating = true;
    vertical_calibrating = true;
    gpio_add_raw_irq_handler_with_order_priority(opto_horizontal, raw_calibration_handler, PICO_HIGHEST_IRQ_PRIORITY);
    gpio_add_raw_irq_handler_with_order_priority(opto_vertical, raw_calibration_handler, PICO_HIGHEST_IRQ_PRIORITY);
    gpio_set_irq_enabled(opto_horizontal, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    gpio_set_irq_enabled(opto_vertical, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true);
    if (!irq_is_enabled(IO_IRQ_BANK0)) irq_set_enabled(IO_IRQ_BANK0, true);

    motor_horizontal->turnSteps(6000);
    motor_vertical->turnSteps(6000);
}

void MotorControl::calibration_handler(Axis axis, bool rise) {
    if (axis == HORIZONTAL) {
        if (!rise) {
            if (!isCalibrated()) {
                motor_horizontal->stop();
                motor_horizontal->resetStepCounter();
                motor_horizontal->setDirection(CLOCKWISE);
                horizontal_calibrated = true;
                horizontal_calibrating = false;
            }
        }
    }
    if (axis == VERTICAL) {
        if (!rise) {
            if (!isCalibrated()) {
                motor_vertical->stop();
                motor_vertical->resetStepCounter();
                motor_vertical->setDirection(CLOCKWISE);
                // motor_vertical->turnSteps(100);
                vertical_calibrated = true;
                vertical_calibrating = false;
            }
        }
    }  
}