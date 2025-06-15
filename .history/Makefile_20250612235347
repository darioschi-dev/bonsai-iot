# Makefile con supporto ambienti esp32-test, esp32-prod

.PHONY: all build upload uploadfs monitor test wokwi clean flash setup-config config

# Ambiente attivo (default = esp32-prod)
ENV ?= esp32-prod

# Comando principale
all: build upload uploadfs monitor

# Compila firmware
build:
	pio run -e $(ENV)

# Upload firmware (salta in test)
upload:
	@if [ "$(ENV)" != "esp32-test" ]; then \
		pio run -e $(ENV) --target upload; \
	else \
		echo "⏭️  Upload saltato (ambiente test)"; \
	fi

# Upload SPIFFS (salta in test)
uploadfs:
	@if [ "$(ENV)" != "esp32-test" ]; then \
		pio run -e $(ENV) --target uploadfs; \
	else \
		echo "⏭️  Upload SPIFFS saltato (ambiente test)"; \
	fi

# Monitor seriale
monitor:
	pio device monitor

# Copia config.{ENV}.json in config.json
config:
	@echo "🌐 Ambiente attivo: $(ENV)"
	ENV_NAME=$(subst esp32-,,$(ENV)) && \
	if [ -f "data/config.$$ENV_NAME.json" ]; then \
		cp data/config.$$ENV_NAME.json data/config.json; \
		echo "✔️  Configurazione caricata: config.$$ENV_NAME.json"; \
	else \
		echo "❌ Configurazione mancante: config.$$ENV_NAME.json"; \
		exit 1; \
	fi

# Build + config + upload + monitor
flash: config build upload uploadfs monitor

# Solo setup del config reale (default)
setup-config:
	python3 scripts/setup_config.py

# Pulizia build
clean:
	pio run -e $(ENV) --target clean
	
# Test Wokwi
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
	wokwi-cli . && \
	( open "https://wokwi.com/projects/433590215533216769" || xdg-open "https://wokwi.com/projects/433590215533216769" || echo "🌐 Apri manualmente: https://wokwi.com/projects/433590215533216769" )
