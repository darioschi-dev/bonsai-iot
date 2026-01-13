#include "pump_controller.h"

PumpController::PumpController(int pin, unsigned long maxRunMs) 
    : pumpPin_(pin), state_(false), lastChangeMs_(0), 
      startMs_(0), maxRunMs_(maxRunMs), emergencyStop_(false) {}

void PumpController::begin() {
    pinMode(pumpPin_, OUTPUT);
    digitalWrite(pumpPin_, PUMP_OFF);  // OFF di default (LOW = ON per relay)
    state_ = false;
    lastChangeMs_ = millis();
    startMs_ = 0;
    emergencyStop_ = false;
}

bool PumpController::turnOn() {
    if (state_) return false;  // Già accesa
    if (emergencyStop_) return false;  // Emergency stop active
    
    digitalWrite(pumpPin_, PUMP_ON);
    state_ = true;
    lastChangeMs_ = millis();
    startMs_ = millis();
    return true;
}

bool PumpController::turnOff() {
    if (!state_) return false;  // Già spenta
    digitalWrite(pumpPin_, PUMP_OFF);
    state_ = false;
    lastChangeMs_ = millis();
    startMs_ = 0;
    return true;
}

void PumpController::loop() {
    // Failsafe: Emergency stop if pump runs too long
    if (state_ && startMs_ > 0) {
        unsigned long runningTime = millis() - startMs_;
        if (runningTime > maxRunMs_) {
            // EMERGENCY STOP
            digitalWrite(pumpPin_, PUMP_OFF);
            state_ = false;
            emergencyStop_ = true;
            lastChangeMs_ = millis();
            startMs_ = 0;
            
            Serial.println("[PUMP] EMERGENCY STOP! Max runtime exceeded.");
        }
    }
}

unsigned long PumpController::getRunningTimeMs() const {
    if (!state_ || startMs_ == 0) return 0;
    return millis() - startMs_;
}

void PumpController::clearEmergencyStop() {
    emergencyStop_ = false;
    Serial.println("[PUMP] Emergency stop cleared.");
}

void PumpController::setState(bool on) {
    digitalWrite(pumpPin_, on ? PUMP_ON : PUMP_OFF);
    state_ = on;
    lastChangeMs_ = millis();
    if (on) {
        startMs_ = millis();
    } else {
        startMs_ = 0;
    }
}

