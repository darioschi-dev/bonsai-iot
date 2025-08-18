#include "FirmwareUpdateStrategy.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
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

// rileva https
static bool isHttpsUrl_(const String& url) {
  return url.startsWith("https://") || url.startsWith("HTTPS://");
}

// client http/https
static void makeHttpClientForUrl_(const String& url, WiFiClient*& plain, WiFiClientSecure*& tls) {
  plain = nullptr;
  tls   = nullptr;
  if (isHttpsUrl_(url)) {
    auto* c = new WiFiClientSecure();
    c->setInsecure();                 // TODO: caricare CA del tuo dominio in prod
    tls = c;
  } else {
    plain = new WiFiClient();
  }
}

// Versione corrente incorporata nel bin (prima FIRMWARE_VERSION, poi app_desc)
static String currentAppVersion() {
#ifdef FIRMWARE_VERSION
  if (strlen(FIRMWARE_VERSION) > 0) return String(FIRMWARE_VERSION);
#endif
  const esp_app_desc_t* app = esp_ota_get_app_description();
  if (app && app->version[0] != '\0') return String(app->version);
  return String("unknown");
}

// versione “sospetta” => salta update per evitare loop
static bool isBadVersion_(const String& v) {
  return v.length() == 0 || v == "v0.0.0" || v == "0.0.0" || v == "unknown";
}

// confronto semver “x.y.z”; se non è semver, fallback confronto stringhe
static int compareVersions_(const String& a, const String& b) {
  int a1=0,a2=0,a3=0, b1=0,b2=0,b3=0;
  int gotA = sscanf(a.c_str(), "%d.%d.%d", &a1,&a2,&a3);
  int gotB = sscanf(b.c_str(), "%d.%d.%d", &b1,&b2,&b3);
  if (gotA==3 && gotB==3) {
    if (a1!=b1) return (a1<b1)?-1:+1;
    if (a2!=b2) return (a2<b2)?-1:+1;
    if (a3!=b3) return (a3<b3)?-1:+1;
    return 0;
  }
  return a.compareTo(b); // <0 a<b ; 0 eq ; >0 a>b
}

// Se nel config non è settato ota_manifest_url, costruiscilo da update_server
static String computeManifestUrl() {
  if (config.ota_manifest_url.length() > 0) return config.ota_manifest_url;
  String base = config.update_server; // es. "https://bonsai-iot-update.darioschiavano.it"
  if (base.endsWith("/")) base.remove(base.length() - 1);
  return base + "/firmware/manifest.json";
}

// Scarica manifest e ne estrae version + url
static bool fetchManifest(String& outVersion, String& outUrl) {
  const String manifestUrl = computeManifestUrl();
  if (manifestUrl.isEmpty()) return false;

  WiFiClient* plain = nullptr;
  WiFiClientSecure* tls = nullptr;
  makeHttpClientForUrl_(manifestUrl, plain, tls);

  HTTPClient http;
  bool begun = false;
  if (tls) begun = http.begin(*tls, manifestUrl);
  else     begun = http.begin(*plain, manifestUrl);
  if (!begun) {
    delete plain; delete tls;
    return false;
  }

  int code = http.GET();
  if (code != 200) {
    if (config.debug) Serial.printf("[OTA] Manifest GET=%d\n", code);
    http.end();
    delete plain; delete tls;
    return false;
  }

  StaticJsonDocument<2048> doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  delete plain; delete tls;
  if (e) {
    if (config.debug) Serial.printf("[OTA] Manifest JSON error: %s\n", e.c_str());
    return false;
  }

  outVersion = doc["version"].as<String>();
  outUrl     = doc["url"].as<String>();

  if (outVersion.isEmpty() || outUrl.isEmpty()) return false;
  return true;
}

// -------- API -------------------------------------------------------------

bool FirmwareUpdateStrategy::checkForUpdate() {
  g_cachedNewV = "";
  g_cachedUrl  = "";
  g_hasUpdate  = false;

  const String curV = currentAppVersion();
  if (config.debug) Serial.printf("[OTA] cur=%s\n", curV.c_str());

  // evita loop se versione corrente è sospetta
  if (isBadVersion_(curV)) {
    if (config.debug) Serial.println("[OTA] Versione corrente sospetta (skip check per evitare loop)");
    return false;
  }

  String newV, url;
  if (!fetchManifest(newV, url)) {
    if (config.debug) Serial.println("[OTA] Manifest non scaricabile/valido.");
    return false;
  }

  if (config.debug) Serial.printf("[OTA] new=%s\n", newV.c_str());

  // confronto robusto
  int cmp = compareVersions_(curV, newV);
  if (cmp >= 0) { // cur >= new → niente update
    if (config.debug) Serial.println("[OTA] Nessun aggiornamento disponibile (versione non superiore).");
    return false;
  }

  // Cache per performUpdate()
  g_cachedNewV = newV;
  g_cachedUrl  = url;
  g_hasUpdate  = true;

  if (config.debug) {
    Serial.printf("🟡 Aggiornamento firmware disponibile → %s\n", g_cachedNewV.c_str());
    Serial.printf("[OTA] URL: %s\n", g_cachedUrl.c_str());
  }
  return true;
}

bool FirmwareUpdateStrategy::performUpdate() {
  // Se chiamato senza check positivo, prova a ricaricare il manifest
  if (!g_hasUpdate) {
    if (config.debug) Serial.println("[OTA] performUpdate() senza cache valida. Provo a leggere il manifest...");
    String newV, url;
    if (!fetchManifest(newV, url)) {
      if (config.debug) Serial.println("[OTA] Manifest non disponibile. Update annullato.");
      return false;
    }
    const String curV = currentAppVersion();
    int cmp = compareVersions_(curV, newV);
    if (cmp >= 0) {
      if (config.debug) Serial.println("[OTA] Versione già aggiornata o non superiore. Nessun update.");
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

  // Scegli client http/https per HTTPUpdate
  WiFiClient* plain = nullptr;
  WiFiClientSecure* tls = nullptr;
  makeHttpClientForUrl_(g_cachedUrl, plain, tls);

  t_httpUpdate_return ret;
  if (tls) {
    ret = httpUpdate.update(*tls, g_cachedUrl);
  } else {
    ret = httpUpdate.update(*plain, g_cachedUrl);
  }

  delete plain; delete tls;

  if (ret == HTTP_UPDATE_OK) {
    if (config.debug) Serial.println("[OTA] Flash OK. Riavvio...");
    // marcatura "valid" avverrà al boot successivo in setup():
    // esp_ota_mark_app_valid_cancel_rollback();
    return true; // ESP.restart() lo gestisce internamente in molti casi
  }

  if (config.debug) {
    Serial.printf("[OTA] FAIL err(%d): %s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
  return false;
}
