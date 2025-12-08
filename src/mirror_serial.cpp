#include "mirror_serial.h"

MirrorSerialClass MirrorSerial;

void MirrorSerialClass::begin(unsigned long baud) {
    Serial.begin(baud);
}

size_t MirrorSerialClass::write(uint8_t b) {
    Serial.write(b);   // passa anche al Serial hardware
    return 1;
}

