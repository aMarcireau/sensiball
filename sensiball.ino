#include <SoftwareSerial.h>

/// pins declarations
const int rotate_direction_pin = 7;
const int rotate_pwm_pin = 9;
const int rotate_vertical_switch_pin = 6;
const int slide_direction_pin = 7;
const int slide_pwm_pin = 9;
const int slide_outer_switch_pin = 1;
const int slide_inner_switch_pin = 0;
const int slide_forward_encoder_pin = 2;
const int slide_backward_encoder_pin = 3;

/// constants
const unsigned char verticalize_rotate_speed = 255; // in integer range [0, 255]
const unsigned char verticalize_rotate_speed_divider = 2;
const float slide_proportional_gain = 0.32;
const float slide_integral_gain = 0.1;
const float slide_derivative_gain = 0.3;
const float slide_timestep = 100; // ms

/// state variables
unsigned char rotate_speed = 0;
bool is_rotate_clockwise = false;
int slide_minimum_pulses = 0;
int slide_maximum_pulses = 0;
volatile int volatile_slide_pulses = 0;
float slide_target = 0.5; //  in range [0, 1]
unsigned long slide_previous_update_time = 0; // ms
float slide_previous_error = 0;
float slide_total_error = 0;

/// forward_change is called when the forward encoder's value changes.
void forward_change() {
    if(digitalRead(slide_backward_encoder_pin) == 0) {
        if (digitalRead(slide_forward_encoder_pin) == 0) {
            ++volatile_slide_pulses;
        } else {
            --volatile_slide_pulses;
        }
    } else {
        if (digitalRead(slide_forward_encoder_pin) == 0) {
            --volatile_slide_pulses;
        } else {
            ++volatile_slide_pulses;
        }
    }
}

/// backward_change is called when the backward encoder's value changes.
void backward_change() {
    if(digitalRead(slide_forward_encoder_pin) == 0) {
        if (digitalRead(slide_backward_encoder_pin) == 0) {
            --volatile_slide_pulses;
        } else {
            ++volatile_slide_pulses;
        }
    } else {
        if (digitalRead(slide_backward_encoder_pin) == 0) {
            ++volatile_slide_pulses;
        } else {
            --volatile_slide_pulses;
        }
    }
}

/// rotate sets the rotation speed and direction.
void rotate(unsigned char speed, bool is_clockwise) {
    rotate_speed = speed;
    is_rotate_clockwise = is_clockwise;
    digitalWrite(rotate_direction_pin, is_rotate_clockwise ? HIGH : LOW);
    analogWrite(rotate_pwm_pin, rotate_speed);
}

/// verticalize moves the players to vertical position.
void verticalize() {
    unsigned char target_rotate_speed = verticalize_rotate_speed;
    bool target_is_clockwise = is_rotate_clockwise;
    while (digitalRead(rotate_vertical_switch_pin) == HIGH) {
        rotate(target_rotate_speed, target_is_clockwise);
        while (digitalRead(rotate_vertical_switch_pin) == HIGH) {}
        rotate(0, false);
        target_rotate_speed /= verticalize_rotate_speed_divider;
        target_is_clockwise = !target_is_clockwise;
    }
}

/// setup runs once on boot.
void setup() {

    // setup pins and interrupts
    pinMode(rotate_direction_pin, OUTPUT);
    pinMode(rotate_vertical_switch_pin, INPUT_PULLUP);
    pinMode(slide_outer_switch_pin, INPUT_PULLUP);
    pinMode(slide_inner_switch_pin, INPUT_PULLUP);
    pinMode(slide_forward_encoder_pin, INPUT);
    pinMode(slide_backward_encoder_pin, INPUT);
    attachInterrupt(0, forward_change, CHANGE);
    attachInterrupt(1, backward_change, CHANGE);

    // initialize slide pulse limits
    digitalWrite(slide_direction_pin, HIGH);
    analogWrite(slide_pwm_pin, 255);
    while (digitalRead(slide_outer_switch_pin) == HIGH) {}
    analogWrite(slide_pwm_pin, 0);
    digitalWrite(slide_direction_pin, LOW);
    noInterrupts();
    slide_minimum_pulses = volatile_slide_pulses;
    interrupts();
    analogWrite(slide_pwm_pin, 50);
    while (digitalRead(slide_inner_switch_pin) == HIGH) {}
    analogWrite(slide_pwm_pin, 0);
    noInterrupts();
    slide_maximum_pulses = volatile_slide_pulses;
    interrupts();

    verticalize(); // @DEV
}

/// loop runs over and over.
void loop() {
    noInterrupts();
    int slide_pulses = volatile_slide_pulses;
    interrupts();

    // manage slide command
    unsigned long current_time = millis();
    if (current_time - slide_previous_update_time >= slide_timestep) {
        slide_previous_update_time = current_time;
        float pulse_slope = 1.0f / (slide_maximum_pulses - slide_minimum_pulses);
        float pulse_intercept = -pulse_slope * slide_minimum_pulses;
        float error = slide_target - (slide_pulses * pulse_slope + pulse_intercept);
        slide_total_error += error;
        float command = (
            slide_proportional_gain * error
            + slide_derivative_gain * (error - slide_previous_error)
            + slide_integral_gain * slide_total_error);
        if (command > 0  && digitalRead(slide_outer_switch_pin) == LOW) {
            command = 0;
        }
        if (command < 0 && digitalRead(slide_inner_switch_pin) == LOW) {
            command = 0;
        }
        if (command >= 0) {
            digitalWrite(slide_direction_pin, HIGH);
            analogWrite(slide_pwm_pin, command > 255 ? 255 : (unsigned char)command);
        } else {
            digitalWrite(slide_direction_pin, LOW);
            analogWrite(slide_pwm_pin, command < -255 ? 255 : (unsigned char)(-command));
        }
    }

    // update slide limits
    if (digitalRead(slide_outer_switch_pin) == LOW) {
        slide_maximum_pulses = slide_pulses;
    } else if (slide_pulses >= slide_maximum_pulses) {
        slide_maximum_pulses = slide_pulses + 1;
    }
    if (digitalRead(slide_inner_switch_pin) == LOW) {
        slide_minimum_pulses = slide_pulses;
    } else if (slide_pulses <= slide_minimum_pulses) {
        slide_minimum_pulses = slide_pulses - 1;
    }

    // read commands from high-level driver
}
