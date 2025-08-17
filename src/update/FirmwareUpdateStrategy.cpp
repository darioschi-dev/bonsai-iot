#include "FirmwareUpdateStrategy.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "../config.h"
#include "../version_auto.h"

extern "C" {
  #include "esp_ota_ops.h"
}

extern Config config;

// -------- Cache tra checkForUpdate() e performUpdate() --------
static String g_cachedNewV;
static String g_cachedUrl;
static bool   g_hasUpdate = false;

// -------- Helpers ---------------------------------------------------------

// Versione corrente incorporata nel bin
static String currentAppVersion() {
  const esp_app_desc_t* app = esp_ota_get_app_description();
  if (app && app->version[0] != '\0') {
    return String(app->version);
  }
  // fallback: generata dallo script di build
#ifdef FIRMWARE_VERSION
  return String(FIRMWARE_VERSION);
#else
  return String("unknown");
#endif
}

// Se nel config non è settato ota_manifest_url, costruiscilo da update_server
static String computeManifestUrl() {
  if (config.ota_manifest_url.length() > 0) {
    return config.ota_manifest_url;
  }
  String base = config.update_server; // es. "https://bonsai-iot-update.darioschiavano.it"
  if (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/firmware/manifest.json";
}

// Scarica manifest e ne estrae version + url (+ sha256 opzionale in futuro)
static bool fetchManifest(String& outVersion, String& outUrl) {
  const String manifestUrl = computeManifestUrl();
  if (manifestUrl.isEmpty()) return false;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, manifestUrl)) return false;

  int code = http.GET();
  if (code != 200) { http.end(); return false; }

  StaticJsonDocument<1024> doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) return false;

  // ✅ modo corretto
  outVersion = doc["version"].as<String>();
  outUrl     = doc["url"].as<String>();

  return !(outVersion.isEmpty() || outUrl.isEmpty());
}

// Confronto robusto: se uguali → NO update; se diversi → update.
// (Evita dipendere dal formato: semver, timestamp, ecc.)
static bool isDifferentVersion(const String& newV, const String& curV) {
  return newV != curV;
}

// -------- API -------------------------------------------------------------

bool FirmwareUpdateStrategy::checkForUpdate() {
  g_cachedNewV = "";
  g_cachedUrl  = "";
  g_hasUpdate  = false;

  String newV, url;
  if (!fetchManifest(newV, url)) {
    if (config.debug) Serial.println("[OTA] Manifest non scaricabile/valido.");
    return false;
  }

  const String curV = currentAppVersion();
  if (config.debug) {
    Serial.printf("[OTA] cur=%s, new=%s\n", curV.c_str(), newV.c_str());
  }

  if (!isDifferentVersion(newV, curV)) {
    if (config.debug) Serial.println("[OTA] Nessun aggiornamento disponibile (versioni coincidenti).");
    return false;
  }

  // Cache per performUpdate()
  g_cachedNewV = newV;
  g_cachedUrl  = url;
  g_hasUpdate  = true;

  if (config.debug) {
    Serial.printf("[OTA] Aggiornamento disponibile → %s\n", g_cachedNewV.c_str());
  }
  return true;
}

bool FirmwareUpdateStrategy::performUpdate() {
  // Se chiamato senza check positivo, recupera manifest ora (per robustezza)
  if (!g_hasUpdate) {
    if (config.debug) Serial.println("[OTA] performUpdate() senza cache valida. Provo a leggere il manifest...");
    String newV, url;
    if (!fetchManifest(newV, url)) {
      if (config.debug) Serial.println("[OTA] Manifest non disponibile. Update annullato.");
      return false;
    }
    const String curV = currentAppVersion();
    if (newV == curV) {
      if (config.debug) Serial.println("[OTA] Versione già aggiornata. Nessun update necessario.");
      return false;
    }
    g_cachedNewV = newV;
    g_cachedUrl  = url;
    g_hasUpdate  = true;
  }

  if (config.debug) {
    Serial.printf("[OTA] Avvio update verso %s\n", g_cachedNewV.c_str());
    Serial.printf("[OTA] URL: %s\n", g_cachedUrl.c_str());
  }

  WiFiClientSecure dl;
  dl.setInsecure(); // In prod carica la CA del tuo dominio

  // Esegue l’OTA. HTTPUpdate gestisce stream+flash e imposta la next boot partition.
  t_httpUpdate_return ret = httpUpdate.update(dl, g_cachedUrl);

  if (ret == HTTP_UPDATE_OK) {
    if (config.debug) Serial.println("[OTA] Flash OK. Riavvio...");
    // NB: la marcatura "valid" va fatta al boot successivo:
    //     esp_ota_mark_app_valid_cancel_rollback() in setup().
    return true;
  }

  if (config.debug) {
    Serial.printf("[OTA] FAIL err(%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
  return false;
}
