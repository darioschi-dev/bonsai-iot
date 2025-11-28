#pragma once
#include <vector>
#include <Arduino.h>
#include "UpdateStrategy.h"

class UpdateManager {
    std::vector<UpdateStrategy*> strategies;
    bool rebootRequired = false;

public:
    void registerStrategy(UpdateStrategy* s) {
        strategies.push_back(s);
    }

    void runAll() {
        for (auto* s : strategies) {

            bool need = false;
            try {
                need = s->checkForUpdate();
            } catch (...) {
                Serial.printf("❌ %s: eccezione in checkForUpdate()\n", s->getName());
                continue;
            }

            if (!need) continue;

            Serial.printf("🟡 Update disponibile per %s\n", s->getName());

            bool ok = false;
            try {
                ok = s->performUpdate();
            } catch (...) {
                Serial.printf("❌ %s: eccezione in performUpdate()\n", s->getName());
            }

            if (ok) {
                Serial.printf("✅ Update %s completato\n", s->getName());
                rebootRequired = true;
            } else {
                Serial.printf("❌ Update %s fallito\n", s->getName());
            }
        }

        if (rebootRequired) {
            Serial.println("🔁 Reboot necessario (1s)...");
            delay(1000);
            ESP.restart();
        }
    }
};
