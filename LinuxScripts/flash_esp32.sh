#!/usr/bin/env bash
# Flash complet a l'ESP32 via /dev/ttyUSB1
# Ús: bash flash_esp32.sh [--erase]
#   --erase  esborra tota la flash abans de flashar (útil si hi ha NVS corrupte)

set -e

PORT="/dev/ttyUSB1"

source ~/esp/esp-idf/export.sh > /dev/null 2>&1

if [[ "$1" == "--erase" ]]; then
    echo "Esborrant i flashant a $PORT..."
    idf.py -p "$PORT" erase-flash flash monitor
else
    echo "Flashant a $PORT..."
    idf.py -p "$PORT" flash monitor
fi
