#pragma once

#include "UpdateStrategy.h"
#include <Arduino.h>

class FirmwareUpdateStrategy;

class ConfigUpdateStrategy : public UpdateStrategy {
public:
    ConfigUpdateStrategy(FirmwareUpdateStrategy* fw);

    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Config"; }

private:
    FirmwareUpdateStrategy* fw_;
    String availableVersion_;
    String downloadUrl_;
    bool   hasUpdate_ = false;
};
