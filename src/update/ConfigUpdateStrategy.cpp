#include "ConfigUpdateStrategy.h"
#include "../config.h"
#include <HTTPClient.h>
#include <SPIFFS.h>

// Dichiari che esiste una variabile globale config definita altrove
extern Config config;

bool ConfigUpdateStrategy::checkForUpdate() {
    String url = config.update_server + "/config/check?version=" + config.config_version;
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        return body.indexOf("\"available\":true") != -1;
    }
    return false;
}

bool ConfigUpdateStrategy::performUpdate() {
    String url = config.update_server + "/config/latest";
    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code == 200) {
        File f = SPIFFS.open("/config.json", FILE_WRITE);
        if (!f) return false;
        f.print(http.getString());
        f.close();
        return true;
    }
    return false;
}
