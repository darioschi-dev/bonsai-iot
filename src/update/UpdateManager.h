#pragma once
#include <vector>
#include "UpdateStrategy.h"
#include <Arduino.h>

class UpdateManager {
    std::vector<UpdateStrategy*> strategies;

public:
    void registerStrategy(UpdateStrategy* strategy) {
        strategies.push_back(strategy);
    }

    void runAll() {
        for (auto* s : strategies) {
            bool needUpdate = false;
            try {
                needUpdate = s->checkForUpdate();
            } catch (...) {
                Serial.printf("❌ %s: eccezione in checkForUpdate()\n", s->getName());
                continue;
            }

            if (!needUpdate) continue;

            Serial.printf("🟡 Update disponibile: %s\n", s->getName());

            bool ok = false;
            try {
                ok = s->performUpdate();
            } catch (...) {
                Serial.printf("❌ %s: eccezione in performUpdate()\n", s->getName());
            }

            if (ok) {
                Serial.printf("✅ Update %s completato\n", s->getName());
            } else {
                Serial.printf("❌ Update %s fallito\n", s->getName());
            }
        }
    }
};
