#pragma once
#include "UpdateStrategy.h"
#include <Arduino.h>

class FirmwareUpdateStrategy;

class ConfigUpdateStrategy : public UpdateStrategy {
public:
    explicit ConfigUpdateStrategy(FirmwareUpdateStrategy* fw);

    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Config"; }

private:
    FirmwareUpdateStrategy* fw_;

    String availableVersion_;
    String downloadUrl_;
    String sha256_;
    bool   hasUpdate_ = false;

    bool httpGetToString_(const String& url, String& out);
    int compareVersions_(const String& a, const String& b);
};
