# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Índex

- [Windows](#windows)
- [ESP32](#esp32)
- [Architecture](#architecture)
- [Subsistemes](#subsistemes)
- [Active Objects — events](#active-objects--events)
- [Active Objects — endpoints, WebSocket](#active-objects--endpoints-websocket)
- [Key files](#key-files)
- [Flux de treball](#flux-de-treball)
- [Convencions](#convencions)

## Windows

### Build

```bash
bash LinuxScripts/build_win.sh
```

Output: `build-win/app.exe`. Each run the script:
1. Assembles `web/index.html` from subsystem files (`web/assemble.sh`)
2. Generates `web/index_html.h` (embedded HTML/JS header)
3. Compiles `mongoose/mongoose.c` (always, no incremental check)
4. Compiles and links all `.cpp` files with `g++ -std=c++17 -O1 -Wall -static -lwinmm -lws2_32`

Uses `x86_64-w64-mingw32-g++` (devcontainer cross-compiler) if available, otherwise `g++`.

**Important:** The HTML/JS UI is embedded via `web/index_html.h` into `Common/HttpServer/HttpServer.cpp`. Any JS/HTML change requires a full recompile, app restart, and hard browser refresh (Ctrl+Shift+R).

### Run

```bash
build-win/app.exe TEST_UNITARI      # Test unitari: seqüència IO automatitzada, OK/FALLO per pas
build-win/app.exe TEST_INTEGRACIO   # Integració: UI web a http://localhost:8080
```

L'argument és obligatori (`TEST_UNITARI` | `TEST_INTEGRACIO`); sense ell l'app surt amb error.

## ESP32

### Connectar USB (des de PowerShell a Windows, com a administrador)

```powershell
.\WinScripts\attach_usb.ps1
```

Detecta automàticament el dispositiu FTDI/CP210x, fa `bind --force` si cal i `attach` a docker-desktop. El port apareix com `/dev/ttyUSB0` i `/dev/ttyUSB1` al contenidor.

### Compilar i flashejar

```bash
bash LinuxScripts/flash_esp32.sh           # compila i flasheja via /dev/ttyUSB1
bash LinuxScripts/flash_esp32.sh --erase   # esborra TOTA la flash primer (útil si NVS corrupte / el WiFi no arrenca)
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

SSID i contrasenya es configuren via `idf.py menuconfig` → *UnitatControlEnllumenat* (`WIFI_SSID` / `WIFI_PASSWORD`, definits a `main/Kconfig.projbuild`), o editant directament `sdkconfig`.

### Hardware (ESP-WROVER-KIT V4.1)

**Entrades** — `Platform/HW/InputReader_HW.hpp` → `makeHWInputReader()`:

| ID | GPIO | Nota |
|----|------|------|
| E1 | GPIO34 | Input-only; requereix pull-up extern 10 kΩ a 3.3 V |
| E2 | GPIO35 | Input-only; requereix pull-up extern 10 kΩ a 3.3 V |

**Sortides** — `Platform/HW/OutputWriter_HW.hpp` → `makeGPIOWriter()`:

| ID | GPIO | LED placa |
|----|------|-----------|
| S1 | GPIO4 | Blau |
| S2 | GPIO0 | Vermell (strapping pin; no baixar durant el reset) |
| S3 | GPIO2 | Verd |

GPIOs a evitar: GPIO16/17 (PSRAM), GPIO6–11 (flash SPI), GPIO21 (càmera D7 a la placa WROVER-KIT).

### Injecció de plataforma

| Abstracció | Windows (`main-win/main.cpp`) | ESP32 (`main/main_esp32.cpp`) |
|-----------|----------------------|-------------------------------|
| `IOReader` | `makeWSInputReader()` (integració) · `makeTestReader()` (test unitari) | `makeHWInputReader()` |
| `OutputWriter` | `makeConsoleWriter()` | `makeGPIOWriter()` |

`ActuadorSortides` corre a prioritat 3 (a Windows mode integració i a ESP32); a Windows mode test unitari, aquesta prioritat l'ocupa `TestObserver`. A ESP32 hi ha a més `Blink` a prioritat 1.

## Architecture

**Framework:** QP/C++ with the QV cooperative scheduler (single thread). A separate Mongoose thread handles HTTP/WebSocket I/O.

**Threads:**
- **QV thread (cooperative):** IOReader, ControlEntrades, ActuadorSortides; TestObserver (Windows mode test)
- **Mongoose thread:** HttpServer, access to the per-subsystem shared `*_state` structs (via mutex)
- **External process:** Browser (HTTP + WebSocket)

**Cross-thread data:** Several mutex-protected structs are shared between the QV thread and the Mongoose thread — one per subsystem: `control_entrades_state`, `control_horari_state`, `control_remot_state`, `rellotge_state`, plus `remote_io_state` (remote IO input). `ControlEntradesState control_entrades_state` (defined in `AOs/ControlEntrades/ControlEntrades.cpp`, declared `extern` in `AOs/ControlEntrades/ControlEntradesState.h`) is the representative example. All access is guarded by `control_entrades_state.mtx`. `ControlEntrades` writes `control_entrades_state.inputs`, `control_entrades_state.outputs`, `control_entrades_state.last_edges`, `control_entrades_state.edge_counts` and sets `control_entrades_state.push_pending = true` directly in the poll handler. The Mongoose thread reads `control_entrades_state` and pushes WebSocket messages when `push_pending` is set.

**IOReader injection:** `ControlEntrades` accepts an `IOReader = std::function<void(map<int,bool>&, map<int,bool>&)>` at construction. In test mode `makeTestReader()` (`Test/TestUnitariControlEntrades.hpp`) returns a lambda cycling through `TestStep` scenarios. In remote mode `makeWSInputReader()` (`Platform/RemoteIO/InputReader_WS.hpp`) returns a lambda that reads from `remote_io_state` (mutex-protected, written by the Mongoose thread via WebSocket). On ESP32 `makeHWInputReader()` (`Platform/HW/InputReader_HW.hpp`) reads physical GPIO inputs.

**OutputWriter injection:** `ActuadorSortides` accepts an `OutputWriter = std::function<void(int id, bool actiu)>` at construction. `makeGPIOWriter()` (`Platform/HW/OutputWriter_HW.hpp`) initialises GPIOs and returns a lambda that calls `gpio_set_level`. `makeConsoleWriter()` (`AOs/ControlSortides/ActuadorSortides/OutputWriter_Console.hpp`) returns a lambda that prints to stdout. This removes all `#ifdef ESP_PLATFORM` from the AO itself.

**Event memory:** `InputChangedEvt` and `EdgeDetectedEvt` use static (zero-pool) semantics because they hold `std::vector`/`std::unordered_map` which are incompatible with QP memory pools. This is safe under the QV cooperative scheduler. `ReconfigureEvt` uses a QP memory pool (initialized in `main-win/main.cpp`). Max 16 configs per `ReconfigureEvt`. Remote IO input is not a QP event: the Mongoose thread writes directly to `remote_io_state` (mutex-protected), and `ControlEntrades` reads it via the injected `IOReader` lambda on each poll tick.

**Race condition to be aware of:** After a `PUT /config_inputs` HTTP response is sent, the QV poll timer may fire before `RECONFIGURE_SIG` is processed, emitting a WS push with stale IDs. The JS UI guards against this with `expectedInputIds`.

## Subsistemes

Els AOs s'organitzen en 4 subsistemes:

| Subsistema | AOs | Rol |
|-----------|-----|-----|
| **ControlEntrades** | `ControlEntrades` (+ `TestObserver`, només test Windows) | Lectura d'IO i detecció de flancs |
| **ControlSortides** | `ControlHorari` → `ControlRemot` → `ActuadorSortides` | Cadena de decisió i actuació de sortides |
| **Rellotge** | `Rellotge` | Font de temps compartida (alimenta `ControlHorari`) |
| **Diagnòstic** | `Blink` (ESP32) | Indicador de vida (*heartbeat*), no és lògica de domini |

**Flux de sortides:** `Rellotge` —`RELLOTGE_TICK`→ `ControlHorari` —`OUTPUT_STATE`→ `ControlRemot` —`OUTPUT_RESULT`→ `ActuadorSortides`.

**Frontera Entrades↔Sortides:** `ControlEntrades` subscriu `OUTPUT_RESULT_SIG` (per a `detection_enabled()`). És l'única dependència que travessa la frontera entre subsistemes; és informativa, no de control.

**`Blink` (Diagnòstic):** parpelleja S1 com a *heartbeat*. Comparteix el `gpioWriter` amb `ActuadorSortides` però sobre IDs disjunts (Blink → S1/GPIO4; ActuadorSortides → S2/S3). Si `ActuadorSortides` controlés S1 hi hauria conflicte.

## Active Objects — events

| Priority | AO | Plataforma | Publishes | Subscribes |
|----------|----|------------|-----------|------------|
| 7 | `Rellotge` | ambdues | `RELLOTGE_TICK_SIG` | — |
| 6 | `ControlEntrades` | ambdues | `INPUT_CHANGED_SIG`, `EDGE_DETECTED_SIG` | `RECONFIGURE_SIG`, `OUTPUT_RESULT_SIG` |
| 5 | `ControlRemot` | ambdues | `OUTPUT_RESULT_SIG` | `OUTPUT_STATE_SIG`, `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG`, `CTRL_OUTPUT_DELETE_SIG` |
| 4 | `ControlHorari` | ambdues | `OUTPUT_STATE_SIG` | `RELLOTGE_TICK_SIG` |
| 3 | `ActuadorSortides` | ambdues | — | `OUTPUT_RESULT_SIG` |
| 3 | `TestObserver` | Windows (mode test) | — | `INPUT_CHANGED_SIG`, `EDGE_DETECTED_SIG` |
| 1 | `Blink` | ESP32 | — | — |

### Events QP

| Event | Dades |
|-------|-------|
| `RELLOTGE_TICK_INTERNAL_SIG` | — (time event intern, `QTimeEvt`, sense payload) |
| `RELLOTGE_TICK_SIG` (`RellotgeTickEvt`) | `hour`, `minute`, `wday` (0=dilluns..6=diumenge) |
| `INPUT_CHANGED_SIG` (`InputChangedEvt`) | `inputs` (`map<int,bool>` id→estat) |
| `EDGE_DETECTED_SIG` (`EdgeDetectedEvt`) | `input_ids` (`vector<int>`, IDs amb flanc detectat) |
| `RECONFIGURE_SIG` (`ReconfigureEvt`) | `entries[]` {`id`, `detect_edge`, `detection_always`, `linked_outputs[]`, `n_linked`}, `n_configs` (màx 16) |
| `OUTPUT_STATE_SIG` (`OutputStateEvt`) | `outputs[]` {`id`, `state`}, `n_outputs` (màx 32) |
| `CTRL_OUTPUT_CMD_SIG` (`OutputCmdEvt`) | `output_id`, `activate` |
| `CTRL_OUTPUT_MODE_SIG` (`OutputModeEvt`) | `output_id`, `remote` |
| `CTRL_OUTPUT_RETURN_AUTO_SIG` (`OutputReturnAutoEvt`) | `output_id` (-1 = totes les sortides) |
| `CTRL_OUTPUT_DELETE_SIG` (`OutputDeleteEvt`) | `output_id` |
| `OUTPUT_RESULT_SIG` (`OutputResultEvt`) | `outputs` (`map<int,bool>` id→estat consolidat) |

## Active Objects — endpoints, WebSocket

### `ControlEntrades` (prioritat 6)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `PUT /config_inputs` | `[{"id":2,"detect_edge":"falling","detection_always":false,"linked_outputs":[10]}]` | Publica `RECONFIGURE_SIG` |
| → AO | `WS /ws` (client→servidor) | `{"inputs":{"1":true}}` | Escriu a `remoteIO.inputs`; llegit per l'IOReader en cada poll tick |
| AO → | `WS /ws` push (`se.push_pending`) | `"inputs":{"1":true},"last_edges":[2],"edge_counts":{"2":3}` | Escriu `se.inputs`, `se.last_edges`, `se.edge_counts` |
| AO → | `GET /config_inputs` | `[{"id":2,"detect_edge":"falling","detection_always":false,"linked_outputs":[10]}]` | Lectura de `se.configs[]` |

---

### `ControlRemot` (prioritat 5)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /control_outputs` | `[{"id":1,"action":"activate"}]` | `handleJson` posta `CTRL_OUTPUT_CMD_SIG`, `CTRL_OUTPUT_MODE_SIG`, `CTRL_OUTPUT_RETURN_AUTO_SIG` o `CTRL_OUTPUT_DELETE_SIG` |
| AO → | `WS /ws` push (`cr_state.push_pending`) | `"cs_outputs":{"1":{"state":false,"commanded":true,"result":true,"mode":"REMOTE"}}` | Escriu `cr_state.outputsResult` |

Valors vàlids de `action`: `activate`, `deactivate`, `set_remote`, `set_auto`, `return_auto` (`id:-1` = totes les sortides), `delete`.

---

### `ControlHorari` (prioritat 4)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| → AO | `POST /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Escriu `ch_state.programacioHoraria` + `load_pending=true`; l'AO recarrega al pròxim `RELLOTGE_TICK_SIG` |
| AO → | `GET /programacio_horaria` | `{"dilluns":[{"id":1,"act":"on","time":"08:00"}],...}` | Lectura de `ch_state.programacioHoraria` (no involucra l'AO directament) |

---

### `Rellotge` (prioritat 7)

| Direcció | Endpoint / WS | Format | Efecte |
|----------|--------------|--------|--------|
| AO → | `WS /ws` push (`rellotge_state.push_pending`) | `"time":"14:32","day":"dimarts"` | Escriu `rellotge_state.hour`, `.minute`, `.wday` |

Cap endpoint HTTP envia dades al Rellotge.

---

### `ActuadorSortides` (prioritat 3)

Cap endpoint ni WS. Només consumeix events QP i actua sobre hardware o consola.

## Key files

Només les entrades amb un *perquè* no obvi (els paths que s'expliquen sols no hi són):

- `Common/QP/signals.h` — tots els enums de senyals QP i les definicions dels structs d'event
- `Common/QP/qp_config.hpp` — tunables de QP (`QF_MAX_ACTIVE=32`, `QF_MAX_EPOOL=3`)
- `*State.h` (ControlEntrades, ControlHorari, ControlRemot, Rellotge, RemoteIO) — structs compartits amb mutex entre el fil QP i el fil Mongoose (un per subsistema)
- `Test/TestUnitariControlEntrades.hpp` — `TestObserver` + `verifyStep()` + `makeTestReader()` + globals `g_*` (mode test Windows)
- `main-win/main.cpp` / `main/main_esp32.cpp` — entry points (ESP32: WiFi, stacks FreeRTOS, injecció de plataforma)
- `Platform/HW/InputReader_HW.hpp` / `OutputWriter_HW.hpp` — injecció ESP32: `makeHWInputReader()` (GPIO34→E1), `makeGPIOWriter()` (GPIO4/0/2→S1/S2/S3)

Els diagrames d'arquitectura (un per subsistema) són a `docs/*.drawio`; les convencions visuals, a [`docs/drawio-conventions.md`](docs/drawio-conventions.md).

## Flux de treball

Després de qualsevol modificació de codi (no documentació), sempre en aquest ordre:

1. Compila Windows: `bash LinuxScripts/build_win.sh`
2. Compila ESP32: `source ~/esp/esp-idf/export.sh > /dev/null 2>&1 && idf.py build`
3. Si ambdues compilen sense errors: `git commit -am "..." && git push`

No fer commit si qualsevol de les dues compilacions falla.

## Convencions

- Codi i comentaris en **català**
- Les màquines d'estat segueixen els patrons QP/C++ (HSM jeràrquiques, `Q_TRAN`, `Q_SUPER`, `QM_TRAN`)
- Els events dinàmics s'allotgen via `QF_NEW` i s'alliberen automàticament pel framework
- Vegeu `.claude/plugins/qpcpp-patterns/` per als patrons habituals de QP/C++ al projecte
