#include "pump_controller.h"

PumpController::PumpController(int pin) : pumpPin_(pin), state_(false), lastChangeMs_(0) {}

void PumpController::begin() {
    pinMode(pumpPin_, OUTPUT);
    digitalWrite(pumpPin_, PUMP_OFF);  // OFF di default (LOW = ON per relay)
    state_ = false;
    lastChangeMs_ = millis();
}

bool PumpController::turnOn() {
    if (state_) return false;  // Già accesa
    digitalWrite(pumpPin_, PUMP_ON);
    state_ = true;
    lastChangeMs_ = millis();
    return true;
}

bool PumpController::turnOff() {
    if (!state_) return false;  // Già spenta
    digitalWrite(pumpPin_, PUMP_OFF);
    state_ = false;
    lastChangeMs_ = millis();
    return true;
}

void PumpController::setState(bool on) {
    digitalWrite(pumpPin_, on ? PUMP_ON : PUMP_OFF);
    state_ = on;
    lastChangeMs_ = millis();
}

