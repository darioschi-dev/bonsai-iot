#include "ConfigUpdateStrategy.h"
#include "FirmwareUpdateStrategy.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../config_api.h"

extern Config config;

ConfigUpdateStrategy::ConfigUpdateStrategy(FirmwareUpdateStrategy* fw)
: fw_(fw)
{}

bool ConfigUpdateStrategy::checkForUpdate()
{
    Serial.println("[CFG] Checking manifest…");

    String body;
    HTTPClient http;
    if (!http.begin(config.ota_manifest_url)) {
        Serial.println("[CFG] http.begin fallito");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[CFG] GET manifest fallito: %d\n", code);
        http.end();
        return false;
    }

    body = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, body)) {
        Serial.println("[CFG] JSON manifest invalido");
        return false;
    }

    if (!doc.containsKey("config")) {
        Serial.println("[CFG] Nessuna sezione config");
        return false;
    }

    JsonObject c = doc["config"];
    availableVersion_ = c["version"] | "";
    downloadUrl_      = c["url"]     | "";

    if (availableVersion_.isEmpty() || downloadUrl_.isEmpty()) {
        Serial.println("[CFG] Manifest incompleto");
        return false;
    }

    hasUpdate_ = (availableVersion_ != config.config_version);

    if (hasUpdate_) {
        Serial.printf("[CFG] Update disponibile config %s -> %s\n",
                      config.config_version.c_str(), availableVersion_.c_str());
    }

    return hasUpdate_;
}

bool ConfigUpdateStrategy::performUpdate()
{
    if (!hasUpdate_) return false;

    Serial.println("[CFG] Scarico nuovo config…");

    HTTPClient http;
    if (!http.begin(downloadUrl_)) {
        Serial.println("[CFG] http.begin fallito");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[CFG] GET config.json fallito: %d\n", code);
        http.end();
        return false;
    }

    String json = http.getString();
    http.end();

    if (json.length() < 5) {
        Serial.println("[CFG] JSON troppo corto");
        return false;
    }

    DynamicJsonDocument test(8192);
    if (deserializeJson(test, json)) {
        Serial.println("[CFG] JSON non valido");
        return false;
    }

    Serial.println("[CFG] Applico configurazione + persist…");

    if (!applyAndPersistConfigJson(json, false)) {
        Serial.println("[CFG] errore applicazione config");
        return false;
    }

    config.config_version = availableVersion_;

    Serial.println("[CFG] FATTO (reboot richiesto)");
    return true;
}
