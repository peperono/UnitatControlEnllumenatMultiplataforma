# Convencions de diagrames DrawIO — Arquitectura QP/C++

## Colors

| Color | Hex fill / stroke | Representa |
|---|---|---|
| Blau | `#dae8fc` / `#6c8ebf` | Fil QP / Active Object (`QP::QActive`) |
| Verd | `#d5e8d4` / `#82b366` | Events QP (entrada o sortida) |
| Groc | `#fff2cc` / `#d6b656` | Memòria compartida entre fils (mutex o g_* globals) |
| Rosa | `#ffcccc` / `#36393d` | Fil Mongoose (thread extern) |
| Gris | `#f5f5f5` / `#666666` | Estratègia injectada (callback/strategy injectat al constructor) |
| Blanc | `#ffffff` / `#666666` | Actor extern (Browser, client REST) |
| Forma document | — | Fitxer persistent en disc |

## Fletxes

| Estil | Significat |
|---|---|
| Línia contínua | Transferència de dades: `read`, `write`, `publish`, `WS`, `GET`, `PUT`, `POST` |
| Línia discontínua | Injecció de dependència (callback/estratègia) |
| `endArrow=block;endFill=0` | Herència / implementació |
| Fletxa invertida (`startArrow=classic`, `endArrow=none`) | Dades flueixen cap a l'origen |

## Etiquetes de fletxa

L'etiqueta reflecteix la **dada** que es transfereix d'un component a l'altre (el payload), no el mecanisme. La part dada anomena el contingut transferit: l'event (`RellotgeTickEvt`), els camps (`{time, day}`) o el recurs (`/config_inputs`).

Quan cal etiqueta explícita, es prefixa amb el mecanisme que la mou: `<mecanisme> <dada>` (p. ex. `WS {time, day}`).

**L'etiqueta s'omet quan l'ancoratge de la fletxa ja identifica la dada** — p. ex. una fletxa que surt d'una fila concreta d'un objecte de memòria estructurada (la dada és el camp) o d'una caixa d'event. L'etiqueta explícita es reserva per quan la dada no és evident pels extrems (p. ex. un push `WS` que agrega diversos camps).

| Mecanisme (prefix) | Què mou la dada |
|---|---|
| `publish` | `QF_PUBLISH` — bus QP, tots els subscrits reben l'event |
| `POST` | `QActive::POST` — event directe a un AO, thread-safe |
| `WS` | HttpServer llegeix SharedState i envia el payload per WebSocket |
| `write` | Escriptura del camp/struct a memòria compartida sota mutex |
| `read` | Lectura del camp/struct de memòria compartida sota mutex |

## Estructura d'un Active Object

- Rectangle blau, `verticalAlign=top`, títol en negreta: `Nom (QP::QActive)`
- Sub-rectangles verds a l'interior per a cada event d'entrada i sortida
- Format de l'etiqueta d'event: `NomEvt\n(SIGNAL_NAME, mecanisme)` — dues línies, `fontSize=8`
- Events d'entrada a l'esquerra, events de sortida a la dreta

## IO Strategies (callbacks)

- Rectangle blau amb estereotip `<<abstract>>` per al tipus base (`IOReader`)
- Rectangle blau amb estereotip `<<callback>>` per a cada instància concreta
- Fletxa d'herència (`endArrow=block;endFill=0`) de cada instància cap al tipus base

## Memòria compartida

- Es representa com un **únic** objecte estructurat: un `swimlane` amb `childLayout=stackLayout`
  - El **títol del swimlane** és el nom de l'struct i fa de capçalera (`startSize=26`, color groc `#fff2cc` / `#d6b656`)
  - Cada camp és una **fila filla transparent** (`text` amb `strokeColor=none`, `fillColor=none`), `parent` = id del swimlane
  - No s'usa cap `group` ni cel·les `topButton`/`bottomButton`: el conjunt és un sol element que es mou i selecciona com una unitat
- Les fletxes poden ancorar-se a un **camp concret** (fila filla) per indicar accés a nivell de camp; si s'ancoren a tot l'struct, l'etiqueta ha d'explicitar la dada (vegeu *Etiquetes de fletxa*)
- Apareix sempre que hi hagi accés des de més d'un fil o component
- Inclou mutex explícit si és cross-thread; g_* globals si és same-thread (QV cooperatiu)

## Containers (agrupació per fitxer o mòdul)

- Rectangle buit (`fillColor=none`) amb etiqueta de text a l'exterior superior
- Agrupa tots els components definits en el mateix fitxer o carpeta

## Llegenda canònica

Tot diagrama inclou aquest bloc de llegenda, com a node `text`, sense modificar-ne el text:

```
Blau: Fil QP (QP::QActive)
Verd: Events QP
Groc: Memòria compartida entre fils.
Rosa: Fil Mongoose
Gris: Estratègia injectada
Línia continua: transferència de dades
Línia discontinua: injecció de dependència
```

## Estructura general del diagrama

- Tot dins un `shape=umlFrame` amb títol `NomProjecte / Data`
- Llegenda canònica (vegeu secció anterior) com a node `text` a la cantonada inferior esquerra
- Fletxes ortogonals (`edgeStyle=orthogonalEdgeStyle`) amb punts de routing explícits en creuaments
