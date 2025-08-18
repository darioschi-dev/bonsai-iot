#pragma once
#include "UpdateStrategy.h"
#include <Arduino.h>

class FirmwareUpdateStrategy : public UpdateStrategy {
public:
    bool checkForUpdate() override;
    bool performUpdate() override;
    const char* getName() override { return "Firmware"; }

private:
    // dati presi dal manifest
    String availableVersion_;
    String downloadUrl_;
    String sha256_;
    bool   hasUpdate_ = false;

    // helpers
    String currentVersion_() const;
    bool httpGetToString_(const String& url, String& outBody);
    bool doHttpDownloadAndFlash_(const String& url, const String& sha256);

    static bool isBadVersion_(const String& v);
    static int compareVersions_(const String& a, const String& b);
};
