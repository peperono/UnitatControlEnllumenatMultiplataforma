#!/usr/bin/env bash
# Arrenca el monitor sèrie de l'ESP32
# Ús: bash monitor_esp32.sh

PORT="/dev/ttyUSB1"

source ~/esp/esp-idf/export.sh > /dev/null 2>&1

idf.py -p "$PORT" monitor
