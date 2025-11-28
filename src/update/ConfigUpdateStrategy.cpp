#include "ConfigUpdateStrategy.h"
#include "FirmwareUpdateStrategy.h"
#include "../config.h"

#include <HTTPClient.h>
#include <ArduinoJson.h>

extern Config config;

ConfigUpdateStrategy::ConfigUpdateStrategy(FirmwareUpdateStrategy* fw)
: fw_(fw) {}

bool ConfigUpdateStrategy::httpGetToString_(const String& url, String& out)
{
    HTTPClient http;
    http.setTimeout(5000);

    if (!http.begin(url)) {
        Serial.println("[CFG] http.begin() FAIL");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[CFG] GET FAIL: %d\n", code);
        http.end();
        return false;
    }

    out = http.getString();
    http.end();
    return true;
}

int ConfigUpdateStrategy::compareVersions_(const String& a, const String& b)
{
    if (a.length() == 0 || b.length() == 0) return 0;
    if (a == b) return 0;

    // confronto semplice stringa → più sicuro delle combinazioni numeriche
    return (a < b) ? -1 : 1;
}

bool ConfigUpdateStrategy::checkForUpdate()
{
    Serial.println("[CFG] Checking manifest…");

    if (config.update_server.isEmpty()) {
        Serial.println("[CFG] update_server NON configurato");
        return false;
    }

    String url = config.update_server + "/manifest.json";

    String body;
    if (!httpGetToString_(url, body)) {
        Serial.println("[CFG] Errore GET manifest");
        return false;
    }

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, body)) {
        Serial.println("[CFG] Manifest JSON invalido");
        return false;
    }

    if (!doc.containsKey("config")) {
        Serial.println("[CFG] Sezione config mancante");
        return false;
    }

    JsonObject cfg = doc["config"];

    availableVersion_ = cfg["version"] | "";
    downloadUrl_      = cfg["url"]     | "";
    sha256_           = cfg["sha256"]  | "";

    if (availableVersion_.isEmpty() || downloadUrl_.isEmpty()) {
        Serial.println("[CFG] Manifest config incompleto");
        return false;
    }

    String cur = config.config_version;

    if (compareVersions_(cur, availableVersion_) < 0) {
        Serial.printf("[CFG] Update disponibile: %s -> %s\n",
            cur.c_str(), availableVersion_.c_str()
        );
        hasUpdate_ = true;
    } else {
        Serial.println("[CFG] Nessun update config");
        hasUpdate_ = false;
    }

    return hasUpdate_;
}

bool ConfigUpdateStrategy::performUpdate()
{
    if (!hasUpdate_) return false;

    Serial.println("[CFG] Scarico nuova config…");

    String json;
    if (!httpGetToString_(downloadUrl_, json)) {
        Serial.println("[CFG] Errore download config");
        return false;
    }

    // Ricicliamo il metodo già usato da mqtt.cpp
    extern bool applyConfigJson(const String&);
    bool ok = applyConfigJson(json);

    if (!ok) {
        Serial.println("[CFG] applyConfigJson() FAIL");
        return false;
    }

    Serial.println("[CFG] Config aggiornata OK");

    return true;
}
