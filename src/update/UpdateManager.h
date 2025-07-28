#pragma once
#include <vector>
#include "UpdateStrategy.h"

class UpdateManager {
    std::vector<UpdateStrategy*> strategies;

public:
    void registerStrategy(UpdateStrategy* strategy) {
        strategies.push_back(strategy);
    }

    void runAll() {
        for (auto s : strategies) {
            if (s->checkForUpdate()) {
                Serial.printf("🟡 Update disponibile: %s\n", s->getName());
                if (s->performUpdate()) {
                    Serial.printf("✅ Update %s completato\n", s->getName());
                } else {
                    Serial.printf("❌ Update %s fallito\n", s->getName());
                }
            }
        }
    }
};
