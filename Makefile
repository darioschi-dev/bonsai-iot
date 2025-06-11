# Makefile con supporto ambienti dev, test, prod

.PHONY: all build upload uploadfs monitor test wokwi clean flash setup-config config

# Ambiente attivo (default = dev)
ENV ?= dev

# Comando principale
all: build upload uploadfs monitor

# Compila firmware
build:
	pio run

# Upload firmware (salta in test)
upload:
	@if [ "$(ENV)" != "test" ]; then \
		pio run --target upload; \
	else \
		echo "⏭️  Upload saltato (ambiente test)"; \
	fi

# Upload SPIFFS (salta in test)
uploadfs:
	@if [ "$(ENV)" != "test" ]; then \
		pio run --target uploadfs; \
	else \
		echo "⏭️  Upload SPIFFS saltato (ambiente test)"; \
	fi

# Monitor seriale
monitor:
	pio device monitor

# Copia config.{ENV}.json in config.json
config:
	@echo "🌐 Ambiente attivo: $(ENV)"
	@if [ -f "data/config.$(ENV).json" ]; then \
		cp data/config.$(ENV).json data/config.json; \
		echo "✔️  Configurazione caricata: config.$(ENV).json"; \
	else \
		echo "❌ Configurazione mancante: config.$(ENV).json"; \
		exit 1; \
	fi

# Build + config + upload + monitor
flash: config build upload uploadfs monitor

# Solo setup del config reale (default)
setup-config:
	python3 scripts/setup_config.py

# Pulizia build
clean:
	pio run --target clean

# Test Wokwi
test:
	@echo "🧪 TEST (ENV=test)"
	$(MAKE) ENV=test config
	ENV_NAME=$$(pio project config --json-output | jq -r '.[0][0]' | sed 's/^env://') && \
	FIRMWARE_PATH=".pio/build/$${ENV_NAME}/firmware.bin" && \
	ELF_PATH=".pio/build/$${ENV_NAME}/firmware.elf" && \
	sed -i.bak -E "s|firmware = .*|firmware = '$${FIRMWARE_PATH}'|" wokwi.toml && \
	sed -i.bak -E "s|elf = .*|elf = '$${ELF_PATH}'|" wokwi.toml && \
	pio run && pio run -t buildfs && \
	if [ -f .env ]; then set -a && . .env && set +a; fi && \
	test -f wokwi.toml && wokwi-cli .