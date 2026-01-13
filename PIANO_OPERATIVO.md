# 🎯 Piano Operativo - Firmware ESP32

Obiettivo: allineare il firmware allo stato attuale del codice, chiudere i punti bloccanti individuati in `ANALISI_ARCHITETTURA.md` e rendere le funzionalità già presenti realmente utilizzabili.

---

## Fase 1 – Bootstrap sicuro (config/WiFi)
**Problema:** con `config.json` mancante o SSID vuoto il boot resta bloccato in `setup_wifi()`.  
**Patch 1.1**  
- `src/config_api.cpp`: distinguere “config mancante” (ritorno esplicito) da “config invalido”.  
- `src/main.cpp`: se il config è mancante/SSID vuoto → saltare WiFi e avviare AP o fallback minimale (anche solo log/ret). Aggiungere timeout/backoff nel loop di connessione.  
- Test: boot senza config.json, verifica che il dispositivo non si blocchi e comunichi l’errore via Serial/Telnet.

---

## Fase 2 – Pipeline sensore → API/MQTT
**Problema:** `/api/soil` espone sempre 0 e MQTT pubblica una sola lettura.  
**Patch 2.1**  
- `src/main.cpp`: aggiungere lettura periodica basata su `measurement_interval` (o ritmo minimo) e salvataggio in struct condivisa.  
- `src/webserver.cpp`: leggere i valori correnti (non `globalSoil/globalPerc` statici) e allineare lo stato pompa da `PumpController`.  
- `src/mqtt.cpp`: pubblicare le letture periodiche e mantenere `last_on`/`pump` coerenti.  
- Test: UI locale mostra valori aggiornati, MQTT riceve aggiornamenti ogni intervallo.

---

## Fase 3 – Logica irrigazione e deep sleep
**Problema:** irrigazione automatica eseguita una sola volta in `setup()`, `measurement_interval` ignorato, deep sleep guidato solo da `webserver_timeout`.  
**Patch 3.1**  
- `src/main.cpp`: separare scheduler letture/irrigazione dal timer di deep sleep. Usare `measurement_interval` per decidere quando leggere/irrigare e `sleep_hours` per la durata del sonno.  
- Persistenza stato pompa già gestita via `RTC_DATA_ATTR`: verificare che salvataggi avvengano anche dopo toggle manuali prima dello sleep.  
- Test: con `debug=false` effettua almeno un ciclo lettura/irrigazione prima del deep sleep; con `debug=true` continua a ciclare senza dormire ma misurando.

---

## Fase 4 – Email/Sicurezza
**Problema:** SMTP hardcoded e inutilizzabile.  
**Patch 4.1**  
- `src/mail.cpp` + `data/config.example.json`: spostare host/porta/credenziali nel config, supporto a TLS senza credenziali in chiaro nel codice.  
- Se l’email resta opzionale, documentare bene la disattivazione per default.  
- Test: invio email con config reale, fallback sicuro quando disabilitato.

---

## Backlog (dopo le fasi sopra)
- Telemetria batteria convertita in volt e con media mobile.  
- Miglioramento web UI (indicatori reali, stato OTA) e log Syslog/MQTT con livelli configurabili.  
- Hardening OTA: validazione SHA256 dal manifest (già previsto in struttura), retry/backoff configurabile, metriche di update.
