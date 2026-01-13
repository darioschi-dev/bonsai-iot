#pragma once
#include <Arduino.h>
#include "config.h"

class PumpController {
private:
    int pumpPin_;
    bool state_;  // true = ON, false = OFF
    unsigned long lastChangeMs_;
    unsigned long startMs_;  // When pump was turned on
    unsigned long maxRunMs_;  // Maximum runtime in milliseconds
    bool emergencyStop_;  // Emergency stop flag
    
public:
    PumpController(int pin, unsigned long maxRunMs = 60000);  // Default: 60 seconds
    void begin();
    bool turnOn();
    bool turnOff();
    void loop();  // Must be called regularly to check for failsafe
    bool getState() const { return state_; }
    unsigned long getLastChangeMs() const { return lastChangeMs_; }
    unsigned long getRunningTimeMs() const;
    bool isEmergencyStop() const { return emergencyStop_; }
    void clearEmergencyStop();  // Reset emergency flag after manual intervention
    void setState(bool on);  // Forza stato (per restore dopo wakeup)
};

