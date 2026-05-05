#!/bin/bash
set -e

IDF_DIR="$HOME/esp/esp-idf"
IDF_VERSION="v5.4.1"

if [ -d "$IDF_DIR" ]; then
    echo "ESP-IDF ja instal·lat a $IDF_DIR, s'omet."
else
    echo "Instal·lant ESP-IDF $IDF_VERSION..."
    mkdir -p "$HOME/esp"
    git clone --depth=1 --branch "$IDF_VERSION" --recursive \
        https://github.com/espressif/esp-idf.git "$IDF_DIR"
    "$IDF_DIR/install.sh" esp32
    echo "source $IDF_DIR/export.sh > /dev/null 2>&1" >> ~/.bashrc
    echo "ESP-IDF instal·lat correctament."
fi

echo "Dev container llest. Usa: bash build.sh  o  idf.py build"
