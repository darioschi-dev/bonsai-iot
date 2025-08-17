#include "update/FirmwareUpdateStrategy.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

extern "C" {
  #include "esp_ota_ops.h"
}

extern Config config;

static bool s_otaCheckedThisBoot = false;   // guard per evitare più check nello stesso boot
static const char* NVS_NS = "ota";
static const char* KEY_FAILS = "fail_count";
static const char* KEY_BACKOFF_UNTIL = "backoff_until"; // epoch ms
static const uint8_t MAX_FAILS = 3;         // dopo 3 fallimenti → backoff
static const uint32_t BACKOFF_MS = 24UL * 60UL * 60UL * 1000UL; // 24h

// --- Version helpers ---
static String getCurrentAppVersion() {
  const esp_app_desc_t* app = esp_ota_get_app_description();
  if (app && app->version[0] != '\0') return String(app->version);
#ifdef APP_VERSION
  return String(APP_VERSION);  // fallback se definita via build_flags
#else
  return String("unknown");
#endif
}

static void markAppValid() {
  // Chiamala in setup() una volta per boot
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  if (config.debug) {
    Serial.printf("[OTA] Mark app valid: %s\n", (err == ESP_OK ? "OK" : "ERR"));
  }
}

// --- Backoff / fuse ---
static bool isBackoffActive() {
  Preferences p;
  if (!p.begin(NVS_NS, true)) return false;
  uint64_t until = p.getULong64(KEY_BACKOFF_UNTIL, 0);
  p.end();
  if (!until) return false;
  uint64_t now = millis(); // va bene per finestre entro ~49 giorni di uptime
  // Se hai ULP o deep sleep lunghi, valuta RTC o epoch da NTP.
  return now < until;
}

static void incFailAndMaybeBackoff() {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return;
  uint32_t fails = p.getUInt(KEY_FAILS, 0);
  fails++;
  p.putUInt(KEY_FAILS, fails);
  if (fails >= MAX_FAILS) {
    uint64_t until = (uint64_t)millis() + BACKOFF_MS;
    p.putULong64(KEY_BACKOFF_UNTIL, until);
    if (config.debug) {
      Serial.printf("[OTA] Troppi fallimenti (%u). Backoff per 24h.\n", fails);
    }
  } else if (config.debug) {
    Serial.printf("[OTA] Fallimenti OTA: %u/%u\n", fails, MAX_FAILS);
  }
  p.end();
}

static void resetFailAndBackoff() {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return;
  p.putUInt(KEY_FAILS, 0);
  p.putULong64(KEY_BACKOFF_UNTIL, 0);
  p.end();
}

// --- API da chiamare in setup() SUBITO dopo un boot post-OTA ---
void otaPostBootMarkValidOnce() {
  // In molti flussi basta chiamarla sempre in setup(); è idempotente.
  markAppValid();
}

// --- Trigger principale ---
void triggerFirmwareCheck() {
  if (s_otaCheckedThisBoot) {
    if (config.debug) Serial.println("[OTA] Check già eseguito in questo boot, skip.");
    return;
  }
  s_otaCheckedThisBoot = true;

  if (isBackoffActive()) {
    if (config.debug) Serial.println("[OTA] Backoff attivo, salto controllo aggiornamenti.");
    return;
  }

  if (config.debug) Serial.println("[OTA] Avvio controllo aggiornamento firmware...");

  FirmwareUpdateStrategy updater;

  // 1) Leggi versione disponibile dal manifest SENZA scaricare il bin
  String current = getCurrentAppVersion();
  String available = updater.peekAvailableVersion(); // <--- vedi nota sotto

  if (available.length() == 0) {
    if (config.debug) Serial.println("[OTA] Impossibile leggere versione disponibile (manifest?).");
    incFailAndMaybeBackoff();
    return;
  }

  Serial.printf("[OTA] cur=%s, new=%s\n", current.c_str(), available.c_str());

  // 2) Confronta prima di aggiornare
  if (available == current) {
    if (config.debug) Serial.println("[OTA] Nessun aggiornamento disponibile.");
    resetFailAndBackoff();
    return;
  }

  if (config.debug) Serial.println("[OTA] Nuova versione trovata! Avvio aggiornamento...");

  // 3) Esegui update (che include download, verifica hash, write, set next boot partition)
  bool ok = updater.performUpdateTo(available); // <--- vedi nota sotto

  if (ok) {
    if (config.debug) Serial.println("[OTA] Aggiornamento completato con successo. Riavvio...");
    resetFailAndBackoff();
    delay(1500);
    ESP.restart(); // NB: alcune lib riavviano da sole. Se così fosse, rendi opzionale.
  } else {
    if (config.debug) Serial.println("[OTA] Aggiornamento fallito!");
    incFailAndMaybeBackoff();
  }
}
