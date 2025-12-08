#pragma once
#include <Arduino.h>
#include "config.h"

class PumpController {
private:
    int pumpPin_;
    bool state_;  // true = ON, false = OFF
    unsigned long lastChangeMs_;
    
public:
    PumpController(int pin);
    void begin();
    bool turnOn();
    bool turnOff();
    bool getState() const { return state_; }
    unsigned long getLastChangeMs() const { return lastChangeMs_; }
    void setState(bool on);  // Forza stato (per restore dopo wakeup)
};

