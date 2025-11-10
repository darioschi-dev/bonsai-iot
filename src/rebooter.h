// rebooter.h
#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "esp_system.h"
#include "esp_sleep.h"

enum class RebootSeverity { Normal, DeepClean, Critical };

struct RebootState {
  bool safeMode = false;
  uint32_t bootCount = 0;
  time_t lastBootEpoch = 0;
  time_t nextRebootEarliest = 0; // epoch seconds; 0 = subito
  String lastRebootReason;
};

class Rebooter {
public:
  void begin(const char* nvsNs = "rebooter") {
    _ns = nvsNs;
    prefs.begin(_ns.c_str(), false);
    // boot counter & window
    time_t now = getNow();
    state.bootCount = prefs.getUInt("boot_count", 0) + 1;
    prefs.putUInt("boot_count", state.bootCount);
    state.lastBootEpoch = now;
    // detect crash-loop: >3 boot negli ultimi 10 min
    const time_t lastCrashWinStart = prefs.getLong64("win_start", 0);
    uint32_t winCount = prefs.getUInt("win_count", 0);
    if (now - lastCrashWinStart > 10 * 60) {
      // reset finestra
      prefs.putLong64("win_start", now);
      prefs.putUInt("win_count", 1);
      winCount = 1;
    } else {
      winCount++;
      prefs.putUInt("win_count", winCount);
    }
    if (winCount >= 3) {
      state.safeMode = true; // evita loop: avvia solo servizi minimi
    }
    state.nextRebootEarliest = prefs.getLong64("next_ok", 0);
    state.lastRebootReason = prefs.getString("last_reason", "");
  }

  bool isSafeMode() const { return state.safeMode; }

  // Chiamala quando vuoi riavviare
  bool requestReboot(const String& reason,
                     RebootSeverity sev = RebootSeverity::Normal,
                     uint32_t minUptimeSec = 20) {
    const time_t now = getNow();
    if (millis()/1000 < minUptimeSec && sev != RebootSeverity::Critical) {
      // troppo presto dopo il boot → rifiuta per evitare loop
      return false;
    }
    if (state.safeMode && sev != RebootSeverity::Critical) {
      // in safe mode non riavviamo, a meno che critico
      return false;
    }
    // backoff: rispetta earliest window
    if (state.nextRebootEarliest != 0 && now < state.nextRebootEarliest && sev != RebootSeverity::Critical) {
      return false;
    }

    // persist motivo + prossimo earliest (backoff esponenziale lieve)
    uint32_t strikes = prefs.getUInt("rb_strikes", 0);
    strikes = min<uint32_t>(strikes + 1, 6);
    uint32_t backoff = (sev == RebootSeverity::Normal) ? (15 * strikes)     // 15s, 30s, 45s…
                       : (sev == RebootSeverity::DeepClean ? (60 * strikes) // 1m, 2m, 3m…
                                                         : 0);              // Critical: subito
    prefs.putUInt("rb_strikes", strikes);
    prefs.putLong64("next_ok", now + backoff);
    prefs.putString("last_reason", reason);

    // esegui reboot
    if (sev == RebootSeverity::DeepClean) {
      // “quasi power-cycle”: spegni WiFi se serve, poi dormi 8s
      delay(50);
      esp_sleep_enable_timer_wakeup(8ULL * 1000ULL * 1000ULL);
      esp_deep_sleep_start();
      return true; // non si arriva qui
    } else {
      delay(50);
      ESP.restart();
      return true; // non si arriva qui
    }
  }

  // Chiamala in setup() dopo begin() per loggare motivo reset
  String resetSummary() {
    const esp_reset_reason_t r = esp_reset_reason();
    const char* rr =
      r == ESP_RST_POWERON ? "POWERON" :
      r == ESP_RST_EXT     ? "EXTERNAL" :
      r == ESP_RST_SW      ? "SOFTWARE" :
      r == ESP_RST_PANIC   ? "PANIC" :
      r == ESP_RST_INT_WDT ? "INT_WDT" :
      r == ESP_RST_TASK_WDT? "TASK_WDT" :
      r == ESP_RST_BROWNOUT? "BROWNOUT" :
      r == ESP_RST_DEEPSLEEP? "DEEPSLEEP" : "OTHER";
    String s = "reset_reason="; s += rr;
    s += " boot_count=" + String(state.bootCount);
    s += " safe_mode=" + String(state.safeMode ? "1" : "0");
    s += " last_reason=" + state.lastRebootReason;
    return s;
  }

private:
  time_t getNow() {
    time_t t = 0; time(&t);
    return t; // se l’NTP non è ancora sync, può essere 0 → finestra si “resetta”
  }

  Preferences prefs;
  String _ns;
public:
  RebootState state;
};
