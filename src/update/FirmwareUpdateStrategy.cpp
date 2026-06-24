#include "FirmwareUpdateStrategy.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Update.h>
#include "../config.h"
#include "../mqtt.h"

extern "C" {
  #include "esp_task_wdt.h"
}

extern Config config;

// --------------------------------------------------------
// Costruttore: prende URL dal config
// --------------------------------------------------------
FirmwareUpdateStrategy::FirmwareUpdateStrategy() {
    manifestUrl_ = config.ota_manifest_url;
}

// --------------------------------------------------------
String FirmwareUpdateStrategy::currentVersion_() const {
#ifdef FIRMWARE_VERSION
    return String(FIRMWARE_VERSION); 
#else
    return "0.0.0";
#endif
}

// --------------------------------------------------------
bool FirmwareUpdateStrategy::isBadVersion_(const String& v) {
    return v.length() < 3 || !v.startsWith("v");
}

// --------------------------------------------------------
int FirmwareUpdateStrategy::compareVersions_(const String& a, const String& b)
{
    if (isBadVersion_(a) || isBadVersion_(b)) return 0;

    int ma, mi, pa;
    int mb, mj, pb;
    sscanf(a.c_str()+1, "%d.%d.%d", &ma,&mi,&pa);
    sscanf(b.c_str()+1, "%d.%d.%d", &mb,&mj,&pb);

    if (ma != mb) return (ma < mb) ? -1 : 1;
    if (mi != mj) return (mi < mj) ? -1 : 1;
    if (pa != pb) return (pa < pb) ? -1 : 1;
    return 0;
}

// --------------------------------------------------------
bool FirmwareUpdateStrategy::httpGetToString_(const String& url, String& out)
{
    HTTPClient http;
    http.setTimeout(10000); // 10s timeout to prevent hanging
    
    if (!http.begin(url)) {
        Serial.println("[FW] HTTP begin failed");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[FW] HTTP GET failed: %d\n", code);
        http.end();
        return false;
    }

    out = http.getString();
    http.end();
    return true;
}

// --------------------------------------------------------
bool FirmwareUpdateStrategy::checkForUpdate()
{
    Serial.println("[FW] Checking manifest…");

    String body;
    if (!httpGetToString_(manifestUrl_, body)) {
        Serial.println("[FW] Errore GET manifest");
        return false;
    }

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, body)) {
        Serial.println("[FW] Manifest JSON invalido");
        return false;
    }

    if (!doc.containsKey("firmware")) {
        Serial.println("[FW] Sezione firmware mancante");
        return false;
    }

    JsonObject fw = doc["firmware"];

    availableVersion_ = fw["version"] | "";
    downloadUrl_      = fw["url"]     | "";
    sha256_           = fw["sha256"]  | "";

    if (availableVersion_.isEmpty() || downloadUrl_.isEmpty()) {
        Serial.println("[FW] Manifest incompleto");
        return false;
    }

    String cur = currentVersion_();
    if (compareVersions_(cur, availableVersion_) < 0) {
        Serial.printf("[FW] Update disponibile: %s -> %s\n", cur.c_str(), availableVersion_.c_str());
        hasUpdate_ = true;
    } else {
        Serial.println("[FW] Nessun update firmware");
        hasUpdate_ = false;
    }

    return hasUpdate_;
}

// --------------------------------------------------------
// 🔥 Download OTA con chunk + reset watchdog
// --------------------------------------------------------
bool FirmwareUpdateStrategy::downloadAndFlash_(const String& url, const String& sha256)
{
    WiFiClient client;
    HTTPClient http;
    http.setTimeout(30000); // 30s timeout for firmware download

    if (!http.begin(client, url)) {
        Serial.println("[FW] begin() fallito");
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("[FW] GET fallito: %d\n", code);
        http.end();
        return false;
    }

    int total = http.getSize();          // può essere -1 (chunked)
    WiFiClient* stream = http.getStreamPtr();

    Serial.printf("[FW] Flashing OTA (%d bytes)…\n", total);

    // Se la dimensione è sconosciuta, usa UPDATE_SIZE_UNKNOWN
    if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
        Serial.printf("[FW] Update.begin FALLITO: %s\n", Update.errorString());
        http.end();
        return false;
    }

    const size_t BUF_SIZE = 2048;
    uint8_t buf[BUF_SIZE];

    int written = 0;
    unsigned long lastReset = millis();

    while (http.connected() && (total < 0 || written < total)) {
        size_t avail = stream->available();

        if (avail) {
            if (avail > BUF_SIZE) avail = BUF_SIZE;

            int len = stream->readBytes(buf, avail);
            if (len <= 0) {
                Serial.println("[FW] readBytes <= 0, stop");
                break;
            }

            size_t w = Update.write(buf, len);
            if (w != (size_t)len) {
                Serial.printf("[FW] Errore scrittura flash: %s\n", Update.errorString());
                http.end();
                return false;
            }

            written += len;
        } else {
            // niente da leggere, piccolo respiro
            delay(10);
        }

        // 🔁 reset watchdog e lascia respirare il core
        if (millis() - lastReset > 300) {
            esp_task_wdt_reset();
            yield();
            lastReset = millis();
        }
    }

    if (!Update.end()) {
        Serial.printf("[FW] Update.end() error: %s\n", Update.errorString());
        http.end();
        return false;
    }

    http.end();
    Serial.println("[FW] Flash OK");
    return true;
}

// --------------------------------------------------------
bool FirmwareUpdateStrategy::performUpdate()
{
    if (!hasUpdate_) return false;

    Serial.println("[FW] Inizio aggiornamento…");

    if (!downloadAndFlash_(downloadUrl_, sha256_)) {
        Serial.println("[FW] OTA FALLITO");
        return false;
    }

    Serial.println("[FW] OTA success. Reboot…");
    delay(1000);
    ESP.restart();
    return true;
}
