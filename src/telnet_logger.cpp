#include "telnet_logger.h"
#include <WiFi.h>

static ESPTelnet telnet;

// =====================================================================
// SETUP
// =====================================================================
void setupTelnetLogger(const char* hostName, uint16_t port) 
{
    Serial.printf("[TELNET] Starting on port %u\n", port);

    if (!telnet.begin(port)) {
        Serial.println("[TELNET] begin() failed");
        return;
    }

    telnet.onConnect([](String ip) {
        Serial.printf("[TELNET] Client connected: %s\n", ip.c_str());
        telnet.println("Welcome to bonsai-esp32 Telnet!");
    });

    telnet.onDisconnect([](String ip) {
        Serial.printf("[TELNET] Client disconnected: %s\n", ip.c_str());
    });

    telnet.onInputReceived([](String msg) {
        Serial.printf("[TELNET] Received: %s\n", msg.c_str());
    });

    Serial.println("[TELNET] Ready!");
}

// =====================================================================
// LOOP
// =====================================================================
void loopTelnetLogger() 
{
    telnet.loop();
}

