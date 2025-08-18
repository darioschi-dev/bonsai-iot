#pragma once
#include "UpdateStrategy.h"
#include <Arduino.h>

class ConfigUpdateStrategy : public UpdateStrategy {
public:
    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Config"; }

private:
    String availableVersion_;
    String downloadUrl_;
    bool   hasUpdate_ = false;
};
