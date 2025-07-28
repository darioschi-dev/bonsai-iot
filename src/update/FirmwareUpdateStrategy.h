#pragma once
#include "UpdateStrategy.h"

class FirmwareUpdateStrategy : public UpdateStrategy {
public:
    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Firmware"; }
};
