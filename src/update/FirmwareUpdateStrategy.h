#pragma once

#include "UpdateStrategy.h"
#include <Arduino.h>

class FirmwareUpdateStrategy : public UpdateStrategy {
public:
    FirmwareUpdateStrategy();

    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Firmware"; }

private:
    String manifestUrl_;
    String availableVersion_;
    String downloadUrl_;
    String sha256_;
    bool   hasUpdate_ = false;

    // internals
    bool isBadVersion_(const String& v);
    int compareVersions_(const String& a, const String& b);
    bool httpGetToString_(const String& url, String& out);
    bool downloadAndFlash_(const String& url, const String& sha256);

    String currentVersion_() const;
};
