#pragma once
#include "UpdateStrategy.h"
#include <Arduino.h>

class FirmwareUpdateStrategy : public UpdateStrategy {
public:
    bool checkForUpdate() override;   // parse manifest + confronto + memorizza dettagli
    bool performUpdate() override;    // usa i dettagli memorizzati
    const char* getName() override { return "Firmware"; }

private:
    // Dati catturati da checkForUpdate()
    String availableVersion_;
    String downloadUrl_;
    String sha256_;
    bool   hasUpdate_ = false;

    String currentVersion_() const;   // legge esp_app_desc_t o APP_VERSION
    bool   httpGetToString_(const String& url, String& outBody);
    bool   doHttpDownloadAndFlash_(const String& url, const String& sha256);
};
