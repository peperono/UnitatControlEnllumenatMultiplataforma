#!/usr/bin/env bash
# Arrenca el monitor sèrie de l'ESP32
# Ús: bash monitor_esp32.sh

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="/dev/ttyUSB1"

source ~/esp/esp-idf/export.sh > /dev/null 2>&1

cd "$ROOT"
idf.py -p "$PORT" monitor
