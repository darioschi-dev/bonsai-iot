#pragma once
#include <Arduino.h>
#include <ESPTelnet.h>

void setupTelnetLogger(const char* hostName = "esp32-logger", uint16_t port = 23);
void loopTelnetLogger();
