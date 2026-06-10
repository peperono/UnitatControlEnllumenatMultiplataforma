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

# udevadm fals per a contenidors sense udev (necessari per l'extensió ESP-IDF de VS Code)
if [ ! -f /usr/local/bin/udevadm ]; then
    sudo tee /usr/local/bin/udevadm > /dev/null << 'UDEVADM'
#!/bin/bash
if [[ "$*" == *"info"* && "$*" == *"-e"* ]]; then
    for dev in /dev/ttyUSB* /dev/ttyACM*; do
        [ -e "$dev" ] || continue
        name=$(basename "$dev")
        echo "P: /devices/virtual/$name"
        echo "N: $name"
        echo "E: DEVNAME=$dev"
        echo "E: SUBSYSTEM=tty"
        echo "E: ID_BUS=usb"
        echo ""
    done
fi
UDEVADM
    sudo chmod +x /usr/local/bin/udevadm
fi

echo "Dev container llest. Usa: bash build_win.sh  o  idf.py build"
