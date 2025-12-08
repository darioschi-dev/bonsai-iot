#include "trigger_firmware_check.h"
#include "update/FirmwareUpdateStrategy.h"
#include "config.h"
#include <Arduino.h>
#include <Preferences.h>

extern "C" {
  #include "esp_ota_ops.h"
}

extern Config config;

static bool s_otaCheckedThisBoot = false;   // evita più check nello stesso boot
static const char* NVS_NS = "ota";
static const char* KEY_FAILS = "fail_count";
static const char* KEY_BACKOFF_UNTIL = "backoff_until"; // epoch ms
static const uint8_t MAX_FAILS = 3;
static const uint32_t BACKOFF_MS = 24UL * 60UL * 60UL * 1000UL; // 24h

// --- helpers NVS ---
static bool isBackoffActive() {
    Preferences p;
    if (!p.begin(NVS_NS, true)) return false;
    uint64_t until = p.getULong64(KEY_BACKOFF_UNTIL, 0);
    p.end();

    time_t now = 0;
    time(&now);
    if (now < 1700000000) return false; // NTP non valido

    return (until > 0) && (now < (time_t)until);
}

static void incFailAndMaybeBackoff() {
  Preferences p;
  if (!p.begin(NVS_NS, false)) return;
  uint32_t fails = p.getUInt(KEY_FAILS, 0);
  fails++;
  p.putUInt(KEY_FAILS, fails);
  if (fails >= MAX_FAILS) {
    time_t now = 0; time(&now);
    uint64_t until = (uint64_t) now + 86400; // 24h
    p.putULong64(KEY_BACKOFF_UNTIL, until);
    if (config.debug) Serial.printf("[OTA] Troppi fallimenti (%u). Backoff 24h.\n", fails);
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

// --- trigger OTA ---
void triggerFirmwareCheck() {
  if (s_otaCheckedThisBoot) {
    if (config.debug) Serial.println("[OTA] Check già fatto in questo boot.");
    return;
  }
  s_otaCheckedThisBoot = true;

  if (isBackoffActive()) {
    if (config.debug) Serial.println("[OTA] Backoff attivo, salto check.");
    return;
  }

  FirmwareUpdateStrategy updater;

  if (updater.checkForUpdate()) {
    if (config.debug) Serial.println("[OTA] Nuova versione trovata! Avvio update...");
    if (updater.performUpdate()) {
      if (config.debug) Serial.println("[OTA] Update OK, riavvio...");
      resetFailAndBackoff();
      delay(1500);
      ESP.restart();
    } else {
      if (config.debug) Serial.println("[OTA] Update fallito!");
      incFailAndMaybeBackoff();
    }
  } else {
    if (config.debug) Serial.println("[OTA] Nessun aggiornamento disponibile.");
    resetFailAndBackoff();
  }
}

