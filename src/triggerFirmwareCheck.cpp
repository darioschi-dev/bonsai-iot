#include "update/FirmwareUpdateStrategy.h"
#include "config.h"
#include <Arduino.h>

extern Config config;

void triggerFirmwareCheck() {
    if (config.debug) {
        Serial.println("[OTA] Avvio controllo aggiornamento firmware...");
    }

    FirmwareUpdateStrategy updater;

    if (updater.checkForUpdate()) {
        if (config.debug) {
            Serial.println("[OTA] Nuova versione trovata! Avvio aggiornamento...");
        }
        if (updater.performUpdate()) {
            if (config.debug) {
                Serial.println("[OTA] Aggiornamento completato con successo. Riavvio...");
            }
            delay(2000);
            ESP.restart();
        } else {
            if (config.debug) {
                Serial.println("[OTA] Aggiornamento fallito!");
            }
        }
    } else {
        if (config.debug) {
            Serial.println("[OTA] Nessun aggiornamento disponibile.");
        }
    }
}
