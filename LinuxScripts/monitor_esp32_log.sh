#!/bin/bash
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
bash "$(dirname "$0")/monitor_esp32.sh" | tee "$ROOT/LogResults/esp32.log"
