#pragma once
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

extern int globalSoil;
extern int globalPerc;

void setup_webserver(int pumpPin);
