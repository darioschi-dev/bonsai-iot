#include "ConfigUpdateStrategy.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include "../config.h"
#include "../config_api.h" 

extern Config config;

bool ConfigUpdateStrategy::checkForUpdate() {
    String body;
    HTTPClient http;
    if (!http.begin(config.ota_manifest_url)) {
        Serial.println("[CFG] impossibile inizializzare http");
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

    // Se il manifest ha una sezione config
    if (doc.containsKey("config")) {
        JsonObject cfg = doc["config"];
        availableVersion_ = cfg["version"] | "";
        downloadUrl_      = cfg["url"]     | "";
    }

    if (availableVersion_.isEmpty() || downloadUrl_.isEmpty()) {
        Serial.println("[CFG] manifest config mancante/incompleto");
        return false;
    }

    hasUpdate_ = (availableVersion_ != config.config_version);
    if (hasUpdate_) {
        Serial.printf("🟡 Update disponibile: Config %s -> %s\n",
                      config.config_version.c_str(), availableVersion_.c_str());
    } else {
        Serial.println("ℹ️ Nessun update config disponibile");
    }

    return hasUpdate_;
}

bool ConfigUpdateStrategy::performUpdate() {
    if (!hasUpdate_) return false;

    HTTPClient http;
    if (!http.begin(downloadUrl_)) {
        Serial.println("[CFG] begin() fallito per config.json");
        return false;
    }
    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[CFG] GET config.json fallito: %d\n", code);
        http.end();
        return false;
    }
    String newJson = http.getString();
    http.end();

    if (newJson.length() < 5) {
        Serial.println("[CFG] config scaricato troppo corto");
        return false;
    }

    DynamicJsonDocument testDoc(8192);
    if (deserializeJson(testDoc, newJson)) {
        Serial.println("[CFG] JSON config non valido");
        return false;
    }

    applyAndPersistConfigJson(newJson); // salva su SPIFFS e riavvia
    return true;
}
