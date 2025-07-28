#include "FirmwareUpdateStrategy.h"
#include <ESPhttpUpdate.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "../config.h"

bool FirmwareUpdateStrategy::checkForUpdate() {
    String url = config.update_server + "/firmware/check?version=" + config.firmware_version;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    int code = http.GET();
    if (code == 200) {
        String body = http.getString();
        return body.indexOf("\"available\":true") != -1;
    }
    return false;
}

bool FirmwareUpdateStrategy::performUpdate() {
    String url = config.update_server + "/firmware/esp32.bin";
    WiFiClientSecure client;
    client.setInsecure();
    t_httpUpdate_return ret = ESPhttpUpdate.update(client, url);
    return ret == HTTP_UPDATE_OK;
}
