# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Índex

- [Windows](#windows)
- [ESP32](#esp32)
- [Architecture](#architecture)
- [Active Objects — endpoints, WebSocket i events](#active-objects--endpoints-websocket-i-events)
- [HTTP endpoints](#http-endpoints)
- [Key files](#key-files)
- [Convencions](#convencions)

## Windows

### Build

```bash
bash build.sh
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
.\attach-usb.ps1
```

Detecta automàticament el dispositiu FTDI/CP210x, fa `bind --force` si cal i `attach` a docker-desktop. El port apareix com `/dev/ttyUSB0` i `/dev/ttyUSB1` al contenidor.

### Compilar i flashejar

```bash
bash flash_esp32.sh           # compila i flasheja via /dev/ttyUSB1
bash flash_esp32.sh --erase   # esborra NVS primer (útil si el WiFi no arrenca)
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
bash monitor_esp32.sh
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

**Entrades** — `HWReader/IOReader_HW.hpp` → `makeHWReader()`:

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
| `IOReader` | `makeRemoteReader()` | `makeHWReader()` |
| `OutputWriter` | `makeConsoleWriter()` | `makeGPIOWriter()` |

A ESP32, la prioritat 1 correspon a `ActuadorSortides` (en lloc de `TestObserver`).

## Architecture

**Framework:** QP/C++ with the QV cooperative scheduler (single thread). A separate Mongoose thread handles HTTP/WebSocket I/O.

**Threads:**
- **QV thread (cooperative):** IOReader, DigitalEdgeDetector, Monitor, ActuadorSortides; TestObserver (Windows mode test)
- **Mongoose thread:** HttpServer, access to SharedState (via mutex)
- **External process:** Browser (HTTP + WebSocket)

**Active Objects (priority order):**

| Priority | AO | Plataforma | Publishes | Subscribes |
|----------|----|------------|-----------|------------|
| 6 | `Rellotge` | ambdues | `RELLOTGE_TICK_SIG` | — |
| 5 | `DigitalEdgeDetector` | ambdues | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` | `RECONFIGURE_SIG`, `OUTPUT_RESULT_SIG` |
| 4 | `ControlRemot` | ambdues | `OUTPUT_RESULT_SIG` | `OUTPUT_STATE_SIG`, `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG`, `CTRL_OUTPUT_DELETE_SIG` |
| 3 | `ControlHorari` | ambdues | `OUTPUT_STATE_SIG` | `RELLOTGE_TICK_SIG` |
| 2 | `Monitor` | ambdues | — | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 1 | `ActuadorSortides` | ambdues | — | `OUTPUT_RESULT_SIG` |
| 1 | `TestObserver` | Windows (mode test) | — | `IO_STATE_CHANGED_SIG`, `EDGE_DETECTED_SIG` |

**Cross-thread data:** `SharedState se` (defined in `main.cpp`, declared `extern` in `DigitalEdgeDetector/SharedState.h`) is the only shared data between the QV thread and the Mongoose thread. All access is guarded by `se.mtx`. `DigitalEdgeDetector` writes `se.inputs`, `se.outputs`, `se.last_edges`, `se.edge_counts` and sets `se.push_pending = true` directly in the poll handler. The Mongoose thread reads `se` and pushes WebSocket messages when `push_pending` is set.

**IOReader injection:** `DigitalEdgeDetector` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` (`Test/TestController.hpp`) returns a lambda cycling through `TestStep` scenarios. In remote mode `makeRemoteReader()` (`RemoteIO/IOReader_Remot.hpp`) returns a lambda that reads from `RemoteIOState remoteIO` (mutex-protected, written by the Mongoose thread via WebSocket). On ESP32 `makeHWReader()` (`HWReader/IOReader_HW.hpp`) reads physical GPIO inputs.

**OutputWriter injection:** `ActuadorSortides` accepts an `OutputWriter = std::function<void(int id, bool actiu)>` at construction. `makeGPIOWriter()` (`ActuadorSortides/OutputWriter_HW.hpp`) initialises GPIOs and returns a lambda that calls `gpio_set_level`. `makeConsoleWriter()` (`ActuadorSortides/OutputWriter_Console.hpp`) returns a lambda that prints to stdout. This removes all `#ifdef ESP_PLATFORM` from the AO itself.

**Event memory:** `IOStateEvt` and `EdgeDetectedEvt` use static (zero-pool) semantics because they hold `std::vector`/`std::unordered_map` which are incompatible with QP memory pools. This is safe under the QV cooperative scheduler. `ReconfigureEvt` uses a QP memory pool (initialized in `main.cpp`). Max 16 configs per `ReconfigureEvt`. Remote IO input is not a QP event: the Mongoose thread writes directly to `remoteIO` (mutex-protected), and `DigitalEdgeDetector` reads it via the injected `IOReader` lambda on each poll tick.

**Race condition to be aware of:** After a `PUT /config_inputs` HTTP response is sent, the QV poll timer may fire before `RECONFIGURE_SIG` is processed, emitting a WS push with stale IDs. The JS UI guards against this with `expectedInputIds`.

## Active Objects — endpoints, WebSocket i events

### `DigitalEdgeDetector` (prioritat 3)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `PUT /config_inputs` | `[{"id":2,"logic_positive":true,"detection_always":false,"linked_outputs":[10]}]` | Publica `RECONFIGURE_SIG` |
| → AO | `WS /ws` (client→servidor) | `{"inputs":{"1":true}}` | Escriu a `remoteIO.inputs`; llegit per l'IOReader en cada poll tick |
| AO → | `WS /ws` push (`se.push_pending`) | `"inputs":{"1":true},"last_edges":[2],"edge_counts":{"2":3}` | Escriu `se.inputs`, `se.last_edges`, `se.edge_counts` |
| AO → | `GET /config_inputs` | `[{"id":2,"logic_positive":true,"detection_always":false,"linked_outputs":[10]}]` | Lectura de `se.configs[]` |

| Event | Rol |
|-------|-----|
| `RECONFIGURE_SIG` (`ReconfigureEvt`) | subscrit — recarrega configuració d'entrades |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | subscrit — actualitza `m_commandedOutputs`; estat efectiu de sortides per a `detection_enabled()` |
| `IO_STATE_CHANGED_SIG` (`IOStateEvt`) | publica — inputs/outputs actuals |
| `EDGE_DETECTED_SIG` (`EdgeDetectedEvt`) | publica — IDs d'entrades amb flanc detectat |

---

### `ControlRemot`

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /control_outputs` | `[{"id":1,"action":"activate"}]` | `handleJson` posta `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG` o `CTRL_OUTPUT_DELETE_SIG` |
| AO → | `WS /ws` push (`cr_state.push_pending`) | `"cs_outputs":{"1":{"state":false,"commanded":true,"result":true,"mode":"REMOTE"}}` | Escriu `cr_state.outputsResult` |

Valors vàlids de `action`: `activate`, `deactivate`, `set_remote`, `set_auto`, `return_auto` (`id:-1` = totes les sortides), `delete`.

| Event | Rol |
|-------|-----|
| `OUTPUT_STATE_SIG` (`OutputStateEvt`) | subscrit — rep l'estat real de sortides (des de `ControlHorari`) |
| `CTRL_OUTPUT_CMD_SIG` (`OutputCmdEvt`) | subscrit — ordre activate/deactivate |
| `CTRL_OUTPUT_MODE_SIG` (`OutputModeEvt`) | subscrit — canvi mode AUTO/REMOTE |
| `CTRL_OUTPUT_RETURN_AUTO_SIG` (`OutputReturnAutoEvt`) | subscrit — torna a AUTO (una o totes) |
| `CTRL_OUTPUT_DELETE_SIG` (`OutputDeleteEvt`) | subscrit — elimina una sortida |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | publica — resultat consolidat de totes les sortides |

---

### `ControlHorari`

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Escriu `ch_state.programacioHoraria` + `load_pending=true`; l'AO recarrega al pròxim `RELLOTGE_TICK_SIG` |
| AO → | `GET /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Lectura de `ch_state.programacioHoraria` (no involucra l'AO directament) |

| Event | Rol |
|-------|-----|
| `RELLOTGE_TICK_SIG` (`RellotgeTickEvt`) | subscrit — cada minut: comprova `load_pending` i executa maniobres |
| `OUTPUT_STATE_SIG` (`OutputStateEvt`) | publica — quan hi ha maniobres que coincideixen amb l'hora actual |

---

### `Rellotge`

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| AO → | `WS /ws` push (`rellotge_state.push_pending`) | `"time":"14:32","day":"dimarts"` | Escriu `rellotge_state.hour`, `.minute`, `.wday` |

Cap endpoint HTTP envia dades al Rellotge.

| Event | Rol |
|-------|-----|
| `RELLOTGE_TICK_INTERNAL_SIG` | intern (time event armat en `initial`) — cada 50 ms (= 1 minut simulat) |
| `RELLOTGE_TICK_SIG` (`RellotgeTickEvt`) | publica — hora/minut/dia actuals |

---

### `Monitor` (prioritat 2)

Cap endpoint ni WS interactua directament amb aquest AO. Només consumeix events QP.

| Event | Rol |
|-------|-----|
| `IO_STATE_CHANGED_SIG` (`IOStateEvt`) | subscrit |
| `EDGE_DETECTED_SIG` (`EdgeDetectedEvt`) | subscrit |

---

## HTTP endpoints

- `GET /` — serves embedded HTML/JS page
- `GET /config_inputs` — returns `se.configs[]` as JSON array
  ```json
  [{"id":2,"logic_positive":true,"detection_always":false,"linked_outputs":[10]}]
  ```
- `PUT /config_inputs` — replaces full config array, posts `RECONFIGURE_SIG`. Body: same format as GET response. Max 16 entries (`MAX_CONFIGS`), max 8 linked outputs (`MAX_LINKED`).
- `POST /control_outputs` — posts command events to `ControlRemot` via `handleJson`. Body: array of actions:
  ```json
  [{"id":1,"action":"activate"}]
  ```
  Valid actions: `activate`, `deactivate`, `set_remote`, `set_auto`, `return_auto` (`id:-1` targets all outputs), `delete`.
- `GET /programacio_horaria` — returns `ch_state.programacioHoraria` as JSON
  ```json
  {"dilluns":[{"id":1,"act":"on","time":"08:00"},{"id":1,"act":"off","time":"22:00"}],"dimarts":[...],...}
  ```
- `POST /programacio_horaria` — replaces schedule, sets `ch_state.load_pending`. Body: same format as GET response. `ControlHorari` reloads it on the next `RELLOTGE_TICK_SIG`.
- `WebSocket /ws` — server pushes on any `push_pending` flag (se / cr_state / rellotge_state / log_state):
  ```json
  {"inputs":{"1":true},"last_edges":[2],"edge_counts":{"2":3},
   "time":"14:32","day":"dimarts",
   "cs_outputs":{"1":{"state":false,"commanded":true,"result":true,"mode":"REMOTE"}},
   "log":[{"t":"14:32:01","src":"ControlRemot","sig":"OUTPUT_RESULT_SIG","d":"1=ON(REM)"}]}
  ```
  Client sends to simulate inputs: `{"inputs":{"1":true}}` — written directly to `remoteIO.inputs`. Outputs are not sent via WS; `DigitalEdgeDetector` uses `m_commandedOutputs` (from `OUTPUT_RESULT_SIG`) per a `detection_enabled()`.

## Key files

- `signals.h` — all QP signal enums and event struct definitions
- `DigitalEdgeDetector/SharedState.h` — the shared struct between QV and Mongoose threads
- `InputConfig.h` — `InputConfig` struct: `id`, `logic_positive`, `detection_always`, `linked_outputs`
- `Test/TestController.hpp` — TestObserver AO + verifyStep() + makeTestReader() + g_* globals
- `docs/ControlEntrades.drawio` — entrades architecture diagram
- `docs/ControlSortides.drawio` — sortides architecture diagram (ControlHorari, ControlRemot, ActuadorSortides)
- `qp_config.hpp` — QP tunables (`QF_MAX_ACTIVE=32`, `QF_MAX_EPOOL=3`)
- `main.cpp` — entry point Windows
- `main/main_esp32.cpp` — entry point ESP32 (WiFi, FreeRTOS stacks, makeHWReader, makeGPIOWriter)
- `HWReader/IOReader_HW.hpp` — `makeHWReader()`: GPIO34 → E1
- `ActuadorSortides/OutputWriter_HW.hpp` — `makeGPIOWriter()`: GPIO4/0/2 → S1/S2/S3
- `ActuadorSortides/OutputWriter_Console.hpp` — `makeConsoleWriter()`: printf stdout
- `flash_esp32.sh` — compila i flasheja l'ESP32 via `/dev/ttyUSB1`
- `monitor_esp32.sh` — monitor sèrie interactiu
- `attach-usb.ps1` — connecta USB al contenidor via usbipd (PowerShell, Windows)

El diagrama de referència és `docs/ControlEntrades.drawio`. Les convencions visuals (colors, fletxes, etiquetes, estructura dels nodes) estan documentades a [`docs/drawio-conventions.md`](docs/drawio-conventions.md).

## Convencions

- Codi i comentaris en **català**
- Les màquines d'estat segueixen els patrons QP/C++ (HSM jeràrquiques, `Q_TRAN`, `Q_SUPER`, `QM_TRAN`)
- Els events dinàmics s'allotgen via `QF_NEW` i s'alliberen automàticament pel framework
- Vegeu `.claude/plugins/qpcpp-patterns/` per als patrons habituals de QP/C++ al projecte
