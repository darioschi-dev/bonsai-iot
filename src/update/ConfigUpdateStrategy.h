#pragma once
#include "UpdateStrategy.h"

class ConfigUpdateStrategy : public UpdateStrategy {
public:
    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Config"; }
};
