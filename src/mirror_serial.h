#pragma once
#include <Arduino.h>

class MirrorSerialClass : public Stream {
public:
    void begin(unsigned long baud);
    size_t write(uint8_t b) override;
    int available() override { return Serial.available(); }
    int read() override { return Serial.read(); }
    int peek() override { return Serial.peek(); }
    void flush() override { Serial.flush(); }

private:
    // Non ridefinire Serial!
};

extern MirrorSerialClass MirrorSerial;

