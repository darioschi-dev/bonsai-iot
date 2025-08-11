#include "FirmwareUpdateStrategy.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "../config.h"
#include "../version_auto.h"

extern Config config;

static bool isNewer(const String& newV, const String& curV) {
  int nM=0,nm=0,np=0,cM=0,cm=0,cp=0;
  sscanf(newV.c_str(), "%d.%d.%d", &nM,&nm,&np);
  sscanf(curV.c_str(), "%d.%d.%d", &cM,&cm,&cp);
  if (nM!=cM) return nM>cM;
  if (nm!=cm) return nm>cm;
  return np>cp;
}

static String computeManifestUrl() {
  if (config.ota_manifest_url.length() > 0) {
    return config.ota_manifest_url;
  }
  String base = config.update_server; // es. "http://pi0.local:3000"
  if (base.endsWith("/")) base.remove(base.length()-1);
  return base + "/firmware/manifest.json";
}

static bool fetchManifest(String& outVersion, String& outUrl) {
  String manifestUrl = computeManifestUrl();

  WiFiClientSecure client;
  client.setInsecure(); // usa WiFiClient se HTTP

  HTTPClient http;
  if (!http.begin(client, manifestUrl)) return false;

  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  StaticJsonDocument<768> doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) return false;

  outVersion = doc["version"].as<String>();
  outUrl     = doc["url"].as<String>();
  return !(outVersion.isEmpty() || outUrl.isEmpty());
}

bool FirmwareUpdateStrategy::checkForUpdate() {
  String newV, url;
  if (!fetchManifest(newV, url)) return false;

  const String curV = FIRMWARE_VERSION; // generato dallo script
  if (config.debug) {
    Serial.printf("[OTA] cur=%s, new=%s\n", curV.c_str(), newV.c_str());
  }
  return isNewer(newV, curV);
}

bool FirmwareUpdateStrategy::performUpdate() {
  String newV, url;
  if (!fetchManifest(newV, url)) return false;

  WiFiClientSecure dl;
  dl.setInsecure(); // usa WiFiClient se HTTP

  t_httpUpdate_return ret = httpUpdate.update(dl, url);

  if (ret == HTTP_UPDATE_OK) return true;

  if (config.debug) {
    Serial.printf("[OTA] FAIL err(%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
  return false;
}
