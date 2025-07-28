#!/bin/bash

set -e

BUILD_DIR=".pio/build/esp32dev"
FIRMWARE_BIN="firmware.bin"
DEFAULT_DASHBOARD_PATH="../bonsai-mqtt-dashboard/uploads/firmware"
TIMESTAMP=$(date +"%Y%m%d_%H%M")
VERSIONED_BIN="esp32-${TIMESTAMP}.bin"

read -p "📁 Inserisci il path di destinazione (default: $DEFAULT_DASHBOARD_PATH): " DEST_DIR
DEST_DIR=${DEST_DIR:-$DEFAULT_DASHBOARD_PATH}

echo "🔨 Compilazione firmware con PlatformIO..."
platformio run

if [ ! -f "$BUILD_DIR/$FIRMWARE_BIN" ]; then
    echo "❌ Errore: $FIRMWARE_BIN non trovato in $BUILD_DIR"
    exit 1
fi

mkdir -p "$DEST_DIR"
cp "$BUILD_DIR/$FIRMWARE_BIN" "$DEST_DIR/esp32.bin"
cp "$BUILD_DIR/$FIRMWARE_BIN" "$DEST_DIR/$VERSIONED_BIN"

echo "✅ Firmware copiato in:"
echo "- $DEST_DIR/esp32.bin (ultima versione)"
echo "- $DEST_DIR/$VERSIONED_BIN (versionato)"
