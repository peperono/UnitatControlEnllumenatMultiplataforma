# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Índex

- [Windows](#windows)
- [ESP32](#esp32)
- [Architecture](#architecture)
- [Active Objects — events](#active-objects--events)
- [Active Objects — endpoints, WebSocket](#active-objects--endpoints-websocket)
- [Key files](#key-files)
- [Flux de treball](#flux-de-treball)
- [Convencions](#convencions)

## Windows

### Build

```bash
bash LinuxScripts/build.sh
```

Output: `build-win/app.exe`. Each run the script:
1. Assembles `web/index.html` from subsystem files (`web/assemble.sh`)
2. Generates `web/index_html.h` (embedded HTML/JS header)
3. Compiles `mongoose/mongoose.c` (always, no incremental check)
4. Compiles and links all `.cpp` files with `g++ -std=c++17 -O1 -Wall -static -lwinmm -lws2_32`

Uses `x86_64-w64-mingw32-g++` (devcontainer cross-compiler) if available, otherwise `g++`.

**Important:** The HTML/JS UI is embedded via `web/index_html.h` into `HttpServer/HttpServer.cpp`. Any JS/HTML change requires a full recompile, app restart, and hard browser refresh (Ctrl+Shift+R).

### Run

```bash
./build/app.exe
# 1 → Test mode (automated IO sequence, prints OK/FALLO per step)
# 2 → Remote mode (web UI at http://localhost:8080)
```

## ESP32

### Connectar USB (des de PowerShell a Windows, com a administrador)

```powershell
.\WinScripts\attach-usb.ps1
```

Detecta automàticament el dispositiu FTDI/CP210x, fa `bind --force` si cal i `attach` a docker-desktop. El port apareix com `/dev/ttyUSB0` i `/dev/ttyUSB1` al contenidor.

### Compilar i flashejar

```bash
bash LinuxScripts/flash_esp32.sh           # compila i flasheja via /dev/ttyUSB1
bash LinuxScripts/flash_esp32.sh --erase   # esborra NVS primer (útil si el WiFi no arrenca)
```

Si el port està ocupat:

```bash
fuser /dev/ttyUSB1   # mostra el PID bloquejant
kill <PID>
idf.py -p /dev/ttyUSB1 flash
```

### Monitor sèrie

**Usuari** (terminal interactiu amb TTY):

```bash
bash LinuxScripts/monitor_esp32.sh
```

**Claude Code** (`idf.py monitor` no funciona sense TTY — usar pyserial directament):

```bash
/home/vscode/.espressif/python_env/idf5.4_py3.10_env/bin/python -c "
import serial, sys, time
s = serial.Serial('/dev/ttyUSB1', 115200, timeout=1)
s.setDTR(False); s.setRTS(True); time.sleep(0.1); s.setRTS(False)
end = time.time() + 15
while time.time() < end:
    d = s.read(512)
    if d: sys.stdout.buffer.write(d); sys.stdout.flush()
" | strings
```

- `serial.Serial('/dev/ttyUSB1', 115200, timeout=1)` — obre el port a 115200 bauds; `timeout=1` fa que cada `read()` esperi màxim 1 segon abans de retornar buit.
- `setRTS(True/False)` — manipula la línia RTS del FTDI, connectada al pin EN de l'ESP32: equivalent a prémer el botó EN de la placa. Sense el reset, el dispositiu ja hauria arrencat i no es veuria res.
- `stdout.buffer.write` — escriu bytes raw sense conversió de codificació.
- `| strings` — filtra el flux binari i només mostra seqüències de 4+ caràcters ASCII imprimibles; sense això els bytes de control UART corromprien la sortida.

### Configuració WiFi

SSID i contrasenya es configuren via `idf.py menuconfig` → *Example Connection Configuration*, o editant directament `sdkconfig`.

### Hardware (ESP-WROVER-KIT V4.1)

**Entrades** — `HWReader/InputReader_HW.hpp` → `makeHWInputReader()`:

| ID | GPIO | Nota |
|----|------|------|
| E1 | GPIO34 | Input-only; requereix pull-up extern 10 kΩ a 3.3 V |

**Sortides** — `ActuadorSortides/OutputWriter_HW.hpp` → `makeGPIOWriter()`:

| ID | GPIO | LED placa |
|----|------|-----------|
| S1 | GPIO4 | Blau |
| S2 | GPIO0 | Vermell (strapping pin; no baixar durant el reset) |
| S3 | GPIO2 | Verd |

GPIOs a evitar: GPIO16/17 (PSRAM), GPIO6–11 (flash SPI), GPIO21 (càmera D7 a la placa WROVER-KIT).

### Injecció de plataforma

| Abstracció | Windows (`main.cpp`) | ESP32 (`main/main_esp32.cpp`) |
|-----------|----------------------|-------------------------------|
| `IOReader` | `makeWSInputReader()` | `makeHWInputReader()` |
| `OutputWriter` | `makeConsoleWriter()` | `makeGPIOWriter()` |

A ESP32, la prioritat 1 correspon a `ActuadorSortides` (en lloc de `TestObserver`).

## Architecture

**Framework:** QP/C++ with the QV cooperative scheduler (single thread). A separate Mongoose thread handles HTTP/WebSocket I/O.

**Threads:**
- **QV thread (cooperative):** IOReader, ControlEntrades, Monitor, ActuadorSortides; TestObserver (Windows mode test)
- **Mongoose thread:** HttpServer, access to ControlEntradesState (via mutex)
- **External process:** Browser (HTTP + WebSocket)

**Cross-thread data:** `ControlEntradesState control_entrades_state` (defined in `ControlEntrades/ControlEntrades.cpp`, declared `extern` in `ControlEntrades/ControlEntradesState.h`) is the only shared data between the QV thread and the Mongoose thread. All access is guarded by `control_entrades_state.mtx`. `ControlEntrades` writes `control_entrades_state.inputs`, `control_entrades_state.outputs`, `control_entrades_state.last_edges`, `control_entrades_state.edge_counts` and sets `control_entrades_state.push_pending = true` directly in the poll handler. The Mongoose thread reads `control_entrades_state` and pushes WebSocket messages when `push_pending` is set.

**IOReader injection:** `ControlEntrades` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` (`Test/TestController.hpp`) returns a lambda cycling through `TestStep` scenarios. In remote mode `makeWSInputReader()` (`RemoteIO/InputReader_WS.hpp`) returns a lambda that reads from `remote_io_state` (mutex-protected, written by the Mongoose thread via WebSocket). On ESP32 `makeHWInputReader()` (`HWReader/InputReader_HW.hpp`) reads physical GPIO inputs.

**OutputWriter injection:** `ActuadorSortides` accepts an `OutputWriter = std::function<void(int id, bool actiu)>` at construction. `makeGPIOWriter()` (`ActuadorSortides/OutputWriter_HW.hpp`) initialises GPIOs and returns a lambda that calls `gpio_set_level`. `makeConsoleWriter()` (`ActuadorSortides/OutputWriter_Console.hpp`) returns a lambda that prints to stdout. This removes all `#ifdef ESP_PLATFORM` from the AO itself.

**Event memory:** `InputChangedEvt` and `EdgeDetectedEvt` use static (zero-pool) semantics because they hold `std::vector`/`std::unordered_map` which are incompatible with QP memory pools. This is safe under the QV cooperative scheduler. `ReconfigureEvt` uses a QP memory pool (initialized in `main.cpp`). Max 16 configs per `ReconfigureEvt`. Remote IO input is not a QP event: the Mongoose thread writes directly to `remote_io_state` (mutex-protected), and `ControlEntrades` reads it via the injected `IOReader` lambda on each poll tick.

**Race condition to be aware of:** After a `PUT /config_inputs` HTTP response is sent, the QV poll timer may fire before `RECONFIGURE_SIG` is processed, emitting a WS push with stale IDs. The JS UI guards against this with `expectedInputIds`.

## Active Objects — events

| Priority | AO | Plataforma | Publishes | Subscribes |
|----------|----|------------|-----------|------------|
| 6 | `Rellotge` | ambdues | `RELLOTGE_TICK_SIG` | — |
| 5 | `ControlEntrades` | ambdues | `INPUT_CHANGED_SIG`, `EDGE_DETECTED_SIG` | `RECONFIGURE_SIG`, `OUTPUT_RESULT_SIG` |
| 4 | `ControlRemot` | ambdues | `OUTPUT_RESULT_SIG` | `OUTPUT_STATE_SIG`, `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG`, `CTRL_OUTPUT_DELETE_SIG` |
| 3 | `ControlHorari` | ambdues | `OUTPUT_STATE_SIG` | `RELLOTGE_TICK_SIG` |
| 2 | `Monitor` | ambdues | — | `INPUT_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 1 | `ActuadorSortides` | ambdues | — | `OUTPUT_RESULT_SIG` |
| 1 | `TestObserver` | Windows (mode test) | — | `INPUT_CHANGED_SIG`, `EDGE_DETECTED_SIG` |

### Events QP

| Event | AO | Rol |
|-------|----|-----|
| `RELLOTGE_TICK_INTERNAL_SIG` | `Rellotge` | intern — time event armat en `initial`, cada 50 ms (= 1 minut simulat) |
| `RELLOTGE_TICK_SIG` (`RellotgeTickEvt`) | `Rellotge` | publica — hora/minut/dia actuals |
| `RELLOTGE_TICK_SIG` (`RellotgeTickEvt`) | `ControlHorari` | subscrit — cada minut: comprova `load_pending` i executa maniobres |
| `RECONFIGURE_SIG` (`ReconfigureEvt`) | `ControlEntrades` | subscrit — recarrega configuració d'entrades |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | `ControlEntrades` | subscrit — actualitza `m_commandedOutputs` per a `detection_enabled()` |
| `INPUT_CHANGED_SIG` (`InputChangedEvt`) | `ControlEntrades` | publica — inputs/outputs actuals |
| `EDGE_DETECTED_SIG` (`EdgeDetectedEvt`) | `ControlEntrades` | publica — IDs d'entrades amb flanc detectat |
| `INPUT_CHANGED_SIG` (`InputChangedEvt`) | `Monitor` | subscrit |
| `EDGE_DETECTED_SIG` (`EdgeDetectedEvt`) | `Monitor` | subscrit |
| `OUTPUT_STATE_SIG` (`OutputStateEvt`) | `ControlRemot` | subscrit — rep l'estat real de sortides (des de `ControlHorari`) |
| `CTRL_OUTPUT_CMD_SIG` (`OutputCmdEvt`) | `ControlRemot` | subscrit — ordre activate/deactivate |
| `CTRL_OUTPUT_MODE_SIG` (`OutputModeEvt`) | `ControlRemot` | subscrit — canvi mode AUTO/REMOTE |
| `CTRL_OUTPUT_RETURN_AUTO_SIG` (`OutputReturnAutoEvt`) | `ControlRemot` | subscrit — torna a AUTO (una o totes) |
| `CTRL_OUTPUT_DELETE_SIG` (`OutputDeleteEvt`) | `ControlRemot` | subscrit — elimina una sortida |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | `ControlRemot` | publica — resultat consolidat de totes les sortides |
| `OUTPUT_STATE_SIG` (`OutputStateEvt`) | `ControlHorari` | publica — quan hi ha maniobres que coincideixen amb l'hora actual |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | `ActuadorSortides` | subscrit — activa GPIOs (ESP32) o printf (Windows) |

## Active Objects — endpoints, WebSocket

### `ControlEntrades` (prioritat 5)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `PUT /config_inputs` | `[{"id":2,"logic_positive":true,"detection_always":false,"linked_outputs":[10]}]` | Publica `RECONFIGURE_SIG` |
| → AO | `WS /ws` (client→servidor) | `{"inputs":{"1":true}}` | Escriu a `remoteIO.inputs`; llegit per l'IOReader en cada poll tick |
| AO → | `WS /ws` push (`se.push_pending`) | `"inputs":{"1":true},"last_edges":[2],"edge_counts":{"2":3}` | Escriu `se.inputs`, `se.last_edges`, `se.edge_counts` |
| AO → | `GET /config_inputs` | `[{"id":2,"logic_positive":true,"detection_always":false,"linked_outputs":[10]}]` | Lectura de `se.configs[]` |

---

### `ControlRemot` (prioritat 4)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /control_outputs` | `[{"id":1,"action":"activate"}]` | `handleJson` posta `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG` o `CTRL_OUTPUT_DELETE_SIG` |
| AO → | `WS /ws` push (`cr_state.push_pending`) | `"cs_outputs":{"1":{"state":false,"commanded":true,"result":true,"mode":"REMOTE"}}` | Escriu `cr_state.outputsResult` |

Valors vàlids de `action`: `activate`, `deactivate`, `set_remote`, `set_auto`, `return_auto` (`id:-1` = totes les sortides), `delete`.

---

### `ControlHorari` (prioritat 3)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Escriu `ch_state.programacioHoraria` + `load_pending=true`; l'AO recarrega al pròxim `RELLOTGE_TICK_SIG` |
| AO → | `GET /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Lectura de `ch_state.programacioHoraria` (no involucra l'AO directament) |

---

### `Rellotge` (prioritat 6)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| AO → | `WS /ws` push (`rellotge_state.push_pending`) | `"time":"14:32","day":"dimarts"` | Escriu `rellotge_state.hour`, `.minute`, `.wday` |

Cap endpoint HTTP envia dades al Rellotge.

---

### `Monitor` (prioritat 2)

Cap endpoint ni WS interactua directament amb aquest AO. Només consumeix events QP.

---

### `ActuadorSortides` (prioritat 1)

Cap endpoint ni WS. Només consumeix events QP i actua sobre hardware o consola.

## Key files

- `signals.h` — all QP signal enums and event struct definitions
- `ControlEntrades/ControlEntradesState.h` — the shared struct between QV and Mongoose threads
- `InputConfig.h` — `InputConfig` struct: `id`, `logic_positive`, `detection_always`, `linked_outputs`
- `Test/TestController.hpp` — TestObserver AO + verifyStep() + makeTestReader() + g_* globals
- `docs/ControlEntrades.drawio` — entrades architecture diagram
- `docs/ControlSortides.drawio` — sortides architecture diagram (ControlHorari, ControlRemot, ActuadorSortides)
- `qp_config.hpp` — QP tunables (`QF_MAX_ACTIVE=32`, `QF_MAX_EPOOL=3`)
- `main.cpp` — entry point Windows
- `main/main_esp32.cpp` — entry point ESP32 (WiFi, FreeRTOS stacks, makeHWInputReader, makeGPIOWriter)
- `HWReader/InputReader_HW.hpp` — `makeHWInputReader()`: GPIO34 → E1
- `ActuadorSortides/OutputWriter_HW.hpp` — `makeGPIOWriter()`: GPIO4/0/2 → S1/S2/S3
- `ActuadorSortides/OutputWriter_Console.hpp` — `makeConsoleWriter()`: printf stdout
- `LinuxScripts/flash_esp32.sh` — compila i flasheja l'ESP32 via `/dev/ttyUSB1`
- `LinuxScripts/monitor_esp32.sh` — monitor sèrie interactiu
- `WinScripts/attach-usb.ps1` — connecta USB al contenidor via usbipd (PowerShell, Windows)

El diagrama de referència és `docs/ControlEntrades.drawio`. Les convencions visuals (colors, fletxes, etiquetes, estructura dels nodes) estan documentades a [`docs/drawio-conventions.md`](docs/drawio-conventions.md).

## Flux de treball

Després de qualsevol modificació de codi (no documentació), sempre en aquest ordre:

1. Compila Windows: `bash LinuxScripts/build.sh`
2. Compila ESP32: `source ~/esp/esp-idf/export.sh > /dev/null 2>&1 && idf.py build`
3. Si ambdues compilen sense errors: `git commit -am "..." && git push`

No fer commit si qualsevol de les dues compilacions falla.

## Convencions

- Codi i comentaris en **català**
- Les màquines d'estat segueixen els patrons QP/C++ (HSM jeràrquiques, `Q_TRAN`, `Q_SUPER`, `QM_TRAN`)
- Els events dinàmics s'allotgen via `QF_NEW` i s'alliberen automàticament pel framework
- Vegeu `.claude/plugins/qpcpp-patterns/` per als patrons habituals de QP/C++ al projecte
