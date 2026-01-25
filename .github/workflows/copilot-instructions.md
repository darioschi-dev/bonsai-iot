# Copilot Instructions — Bonsai System (ESP32 + Dashboard)

## Obiettivo
Analizzare e risolvere problemi di blocco del sistema Bonsai (ESP32 + Dashboard),
identificando una root cause dimostrabile, applicando fix minimi e sicuri e
documentando l’intero processo.

Il problema generico da risolvere è:
“L’app si blocca”.

---

## Architettura del sistema

Il sistema è composto da due moduli principali.

### 1) bonsai-dashboard (Raspberry Pi)
- Broker MQTT: Mosquitto
- Backend: Node.js / TypeScript (API, OTA, MQTT)
- Frontend: Vue + Vite
- Database: SQLite (database/bonsai.sqlite)
- Dev: ./dev-start.sh
- Produzione: pm2 start dist/server.js --name bonsai-dashboard
- WebSocket MQTT: porta 9001
- API HTTP: porta 8081

### 2) bonsai-firmware (ESP32)
- Firmware PlatformIO (src/main.cpp)
- Web UI su SPIFFS (data/)
- Configurazione: data/config.json (generata da scripts/setup_config.py)
- MQTT opzionale
- Deep sleep
- Logica pompa / soglia umidità
- OTA e versioning

---

## Regole operative (importanti e vincolanti)

1. Un passo alla volta: proponi una singola modifica o PR alla volta.
2. Prima le evidenze, poi i fix. Non applicare refactor o modifiche strutturali senza una root cause dimostrabile.
3. Niente complessità prematura: evita refactor grandi, riscritture o redesign non necessari.
4. Ogni scelta tecnica deve essere motivata e documentata (README o docs/).
5. Mantieni coerenza con l’architettura esistente del sistema (ESP32 + Dashboard).
6. Evita refactor massivi: se indispensabili, spezzali in più PR piccole e indipendenti.
7. Per ogni logica, calcolo o meccanismo di controllo (retry, throttling, backoff, deduplica),
   definisci passaggi di verifica chiari o test dove possibile.
8. Ogni modifica deve includere obbligatoriamente:
   - motivazione
   - passaggi di test/verifica
   - commit message proposto
9. In produzione sul Raspberry Pi:
   - raccogli prima evidenze (log, metriche, stato servizi)
   - crea un backup completo del progetto (tar.gz)
   - applica solo modifiche minime e reversibili
10. Non cancellare o riscrivere documentazione esistente
    (README, docs/, improvements_dashboard/, ecc.).
11. Se un file o documento è stato cancellato in passato,
    recuperalo dalla history Git invece di reinventarlo.
12. Usa nomi di variabili, funzioni e file chiari e descrittivi; evita abbreviazioni ambigue.
13. Scrivi commenti solo quando il codice non è autoesplicativo.
14. Segui le best practice di sicurezza e stabilità
    (gestione errori, limiti risorse, retry controllati, log non invasivi).
15. Prima di proporre una PR, verifica che:
    - il sistema non introduca nuovi blocchi o loop
    - i passaggi di verifica siano riproducibili
16. Per ogni commit o utilizzo di gh,
    racchiudi sempre il messaggio tra apici singoli ('...') per evitare problemi di parsing della shell. Evitare $, backtick (`), doppi apici ("). Preferire single-line per commit su shell complesse. Usare redirezione di file se necessario per multi-linea.
17. Mantieni sempre allineamento con il file .github/copilot-instructions.md se presente.

---

## Definizione di “blocco”

Classificare il problema in una o più categorie.

A) Crash / restart  
(processo o container muore e riparte)

B) Freeze  
(processo vivo ma non risponde: event-loop stall, deadlock, I/O bloccato)

C) Saturazione risorse  
(RAM, CPU, disco, OOM killer, swap, throttling)

D) Flood / loop  
(MQTT storm, WebSocket loop, log storm, DB write storm)

E) Problema di rete  
(WebSocket MQTT 9001, DNS, Wi-Fi instabile)

F) Solo UI  
(render infinito, watcher in loop, leak di listener)

---

## Check rapido (ordine consigliato)

1. Produzione Raspberry
   - risorse
   - log
   - stato servizi (mosquitto, pm2, porte)

2. Broker MQTT
   - WebSocket 9001 attivo
   - errori, flood, limiti

3. Backend
   - connessione MQTT
   - parsing payload
   - scritture DB
   - endpoint /health e /debug/build

4. Frontend
   - connessione WS MQTT
   - errori console
   - configurazione env

5. Firmware ESP32
   - publish rate
   - reconnect/backoff
   - payload
   - watchdog
   - heap usage

---

## Output richiesti (obbligatori)

Creare nella cartella docs/audit/ (se non esiste, crearla) i file:

- YYYYMMDD-HHMM-INCIDENT_REPORT.md
- YYYYMMDD-HHMM-ROOT_CAUSE.md
- YYYYMMDD-HHMM-FIX_PLAN.md

Formato standard:

Symptoms  
Evidence  
Root cause  
Fix  
Verification  
Follow-ups  

---

## Produzione — comandi base per raccolta evidenze

uptime  
free -h  
df -h  
dmesg -T | tail -n 200  
ps aux --sort=-%mem | head  
ss -tulpn | egrep '(:1883|:9001|:8081|:3000)'  
pm2 status  
pm2 logs --lines 200  
journalctl -u mosquitto --since "2 hours ago" --no-pager  
mosquitto_sub -t 'bonsai/#' -v   (solo per breve periodo)  
curl http://localhost:8081/health  
curl http://localhost:8081/debug/build  

---

## Criteri preferiti per fix minimi

- Throttling e/o deduplica messaggi MQTT
- Backoff reconnect MQTT (backend e/o firmware)
- Correzione gestione WebSocket MQTT nel frontend (subscribe/unsubscribe, retry)
- Limitazione log e rotazione log (log storm)
- Correzione configurazioni:
  - PM2
  - variabili env
  - BASE_URL
  - MQTT_URL
  - VITE_MQTT_URL

---

## Ambiente di produzione

- Raspberry Pi OS aggiornato
- In passato il sistema era dockerizzato, ora non lo è
- Presente script di deploy: deploy_pi.sh
  - copia file
  - riavvia PM2
  - da verificare

Fine.
