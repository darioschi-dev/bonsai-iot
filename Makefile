# Makefile per bonsai-iot (ESP32)
# Usage esempi:
#   make build                 # build standard
#   make release               # build con bump patch + tag (USE_NEXT_VERSION=1)
#   make flash                 # build + upload + uploadfs + monitor
#   make ota                   # upload bin al server OTA (richiede .env OTA_URL)
#   make ota-direct            # OTA diretto con espota (richiede .env OTA_DIRECT_HOST)

.PHONY: all build release upload uploadfs buildfs monitor test wokwi clean flash setup-config config \
        ota ota-local ota-direct requirements test-scripts ports erase

# Ambiente attivo (default = esp32-prod)
ENV ?= esp32-prod
# Deriva il nome breve: esp32-prod -> prod, esp32-test -> test
ENV_NAME := $(patsubst esp32-%,%,$(ENV))

# Percorsi utili
BIN := .pio/build/$(ENV)/firmware.bin
ELF := .pio/build/$(ENV)/firmware.elf

# Server/parametri vari (caricati da .env se presente)
ifneq ("$(wildcard .env)","")
include .env
export
endif
OTA_URL        ?=
OTA_TOKEN      ?=
OTA_DIRECT_HOST?=
OTA_DIRECT_PORT?=
OTA_DIRECT_AUTH?=
BAUD           ?=115200

all: flash

# --- Config di runtime copiando il profilo giusto --------------------------------

config:
	@echo "🌐 Ambiente attivo: $(ENV) (config: data/config.$(ENV_NAME).json)"
	@if [ -f "data/config.$(ENV_NAME).json" ]; then \
		cp data/config.$(ENV_NAME).json data/config.json; \
		echo "✔️  Configurazione caricata"; \
	else \
		echo "❌ Manca data/config.$(ENV_NAME).json"; exit 1; \
	fi

setup-config:
	python3 scripts/setup_config.py

# --- Build ----------------------------------------------------------------------

# Build standard (NO bump/tag)
build: config
	@echo "🔧 Compilazione per ambiente: $(ENV)"
	pio run -e $(ENV)

# Build release con bump patch + tag (USE_NEXT_VERSION=1)
release: config
	@echo "🏷️  Build di rilascio (bump patch + tag) per: $(ENV)"
	USE_NEXT_VERSION=1 pio run -e $(ENV)

buildfs:
	pio run -e $(ENV) --target buildfs

clean:
	pio run -e $(ENV) --target clean

erase:
	@echo "⚠️  Cancello filesystem (SPIFFS/LittleFS) – attenzione!"
	pio run -e $(ENV) --target erase

# --- Upload seriale & FS --------------------------------------------------------

upload:
	@if [ "$(ENV)" != "esp32-test" ]; then \
		pio run -e $(ENV) --target upload; \
	else \
		echo "⏭️  Upload saltato (ambiente test)"; \
	fi

uploadfs:
	@if [ "$(ENV)" != "esp32-test" ]; then \
		pio run -e $(ENV) --target uploadfs; \
	else \
		echo "⏭️  Upload SPIFFS/LittleFS saltato (ambiente test)"; \
	fi

# --- Monitor seriale ------------------------------------------------------------

monitor:
	pio device monitor -b $(BAUD)

ports:
	@pio device list

# --- Comandi composti -----------------------------------------------------------

flash: build upload uploadfs monitor

# --- OTA via HTTP (server backend) ----------------------------------------------

# Upload al server OTA (usa .env: OTA_URL obbligatoria, OTA_TOKEN opzionale)
ota: build
	@if [ -z "$(OTA_URL)" ]; then echo "❌ OTA_URL non impostato (mettilo in .env)"; exit 1; fi
	@if [ ! -f "$(BIN)" ]; then echo "❌ BIN non trovato: $(BIN)"; exit 1; fi
	@VERSION=$$(date +%Y%m%d_%H%M); \
	echo "➡️  Upload $(BIN) -> $(OTA_URL) (version $$VERSION)"; \
	if [ -n "$(OTA_TOKEN)" ]; then \
		curl --fail-with-body -sS -L -m 60 \
			-H "Authorization: Bearer $(OTA_TOKEN)" \
			-F "firmware=@$(BIN)" -F "version=$$VERSION" \
			"$(OTA_URL)" -w "\nHTTP %{http_code}\n"; \
	else \
		curl --fail-with-body -sS -L -m 60 \
			-F "firmware=@$(BIN)" -F "version=$$VERSION" \
			"$(OTA_URL)" -w "\nHTTP %{http_code}\n"; \
	fi

# Upload diretto all'origine locale (bypassa Cloudflare)
ota-local: build
	@VERSION=$$(date +%Y%m%d_%H%M); \
	echo "➡️  Upload locale $(BIN) -> http://127.0.0.1:3000/upload-firmware (version $$VERSION)"; \
	curl --fail-with-body -sS -L -m 60 \
		-F "firmware=@$(BIN)" -F "version=$$VERSION" \
		http://127.0.0.1:3000/upload-firmware -w "\nHTTP %{http_code}\n"

# --- OTA diretto ESP32 (espota) -------------------------------------------------

# .env (opzionale):
#   OTA_DIRECT_HOST=192.168.1.97
#   OTA_DIRECT_PORT=3232
#   OTA_DIRECT_AUTH=secret  (se usi ArduinoOTA.setPassword)
ota-direct:
	@if [ -z "$(OTA_DIRECT_HOST)" ]; then echo "❌ OTA_DIRECT_HOST non impostato (mettilo in .env)"; exit 1; fi
	@PORT=$${OTA_DIRECT_PORT:-3232}; \
	echo "📡 OTA diretto verso $(OTA_DIRECT_HOST):$$PORT"; \
	EXTRA=""; \
	[ -n "$$OTA_DIRECT_PORT" ] && EXTRA="$$EXTRA --upload_flags --port=$$OTA_DIRECT_PORT"; \
	[ -n "$$OTA_DIRECT_AUTH" ] && EXTRA="$$EXTRA --upload_flags --auth=$$OTA_DIRECT_AUTH"; \
	pio run -e esp32-ota -t upload --upload-port $(OTA_DIRECT_HOST) $$EXTRA

# --- Varie ----------------------------------------------------------------------

test:
	@echo "🧪 TEST (ENV=esp32-test)"
	$(MAKE) ENV=esp32-test config
	ENV_NAME=esp32-test && \
	FIRMWARE_PATH=".pio/build/$${ENV_NAME}/firmware.bin" && \
	ELF_PATH=".pio/build/$${ENV_NAME}/firmware.elf" && \
	sed -i.bak -E "s|firmware = .*|firmware = '$${FIRMWARE_PATH}'|" wokwi.toml && \
	sed -i.bak -E "s|elf = .*|elf = '$${ELF_PATH}'|" wokwi.toml && \
	pio run -e $${ENV_NAME} && \
	pio run -e $${ENV_NAME} -t buildfs && \
	if [ -f .env ]; then set -a && . .env && set +a; fi && \
	wokwi-cli . || echo "🌐 Apri manualmente il progetto Wokwi"

requirements:
	@echo "📦 Generazione requirements.txt..."
	@if ! command -v pipreqs >/dev/null 2>&1; then \
		echo "❌ pipreqs non installato. Installa con: pip install pipreqs"; exit 1; fi
	pipreqs --force --encoding=utf-8 --ignore .pio,.venv

test-scripts:
	@echo "🧪 Test script Python"
	@python3 scripts/setup_config.py || (echo "❌ Errore in setup_config.py" && exit 1)
	@python3 scripts/generate_version.py || (echo "❌ Errore in generate_version.py" && exit 1)

# Build release con bump patch + tag (USE_NEXT_VERSION=1)
release: config
	@echo "🏷️  Build di rilascio (bump patch + tag) per: $(ENV)"
	USE_NEXT_VERSION=1 pio run -e $(ENV)
	@NEW_TAG=$$(grep FIRMWARE_VERSION include/version_auto.h | cut -d'"' -f2); \
	if git rev-parse "$$NEW_TAG" >/dev/null 2>&1; then \
		echo "⚠️ Tag $$NEW_TAG già esistente, non creato"; \
	else \
		git tag $$NEW_TAG && echo "✔️ Creato tag $$NEW_TAG"; \
	fi
