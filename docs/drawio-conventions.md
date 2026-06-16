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
| Línia contínua | Transferència de dades: `{dada}` (sense prefix) o amb prefix de transport (`publish`, `POST`, `/ws`, `GET`/`PUT`/`POST`) |
| Línia discontínua | Injecció de dependència (callback/estratègia) |
| `endArrow=block;endFill=0` | Herència / implementació |
| Fletxa invertida (`startArrow=classic`, `endArrow=none`) | Dades flueixen cap a l'origen |

## Etiquetes de fletxa

Totes les etiquetes d'aresta usen **Helvetica 11px** (`fontFamily=Helvetica;fontSize=11`), sense `font-size`/`font-family` inline al text que ho sobreescrigui.

L'etiqueta anomena la **dada** que es transfereix (el payload), amb els camps entre claus `{…}` (p. ex. `{hour, minute, wday}`). La **classe de transport** es marca amb un **prefix**; **si no hi ha prefix, és una transferència de dades plana i el sentit el marca la fletxa** (cap a l'struct = escriptura; des de l'struct = lectura).

| Prefix | Classe de transport | Exemple |
|---|---|---|
| `GET`/`PUT`/`POST /recurs` | endpoint HTTP | `GET /config_inputs` |
| `/ws` | canal WebSocket (sentit per la fletxa: push servidor→client o enviament client→servidor) | `/ws {inputs}` |
| `publish` | `QF_PUBLISH` — bus QP, event a tots els subscrits | `publish {edges}` |
| `POST` | `QActive::POST` — event directe a un AO, thread-safe | `POST {config_inputs}` |
| *(cap prefix)* | transferència de dades plana: memòria compartida sota mutex, GPIO o estratègia injectada | `{programacioHoraria}`, `{inputs}` |

El prefix es **reserva** per a les tres classes de transport (endpoint, WS, event). Una lectura/escriptura de memòria compartida **no** porta `read`/`write`: la direcció de la fletxa ja ho diu i el color groc ja indica que és memòria sota mutex.

L'identificador de la dada ha de ser un **nom canònic i estable**: el mateix nom representa la mateixa dada a totes les capes on apareix (endpoint, clau JSON, atribut d'struct, camp d'event). Quan dos contextos divergeixen a propòsit (p. ex. `edges` a l'event vs `last_edges` a la cache/JSON), és una **frontera deliberada**, no un descuit.

**Quan ometre l'etiqueta del tot:** només si l'ancoratge identifica EXACTAMENT la dada — una fletxa ancorada a una **fila concreta** d'un objecte de memòria estructurada (la fila *és* el camp). Una **caixa d'event NO és prou** (identifica el *tipus*, no el camp): les fletxes des de/cap a caixes d'event SÍ porten etiqueta amb prefix (`publish {edges}`, `POST {config_inputs}`). Si la fletxa s'ancora a **tot un struct**, també cal etiqueta amb el conjunt (p. ex. `{inputs, last_edges, edge_counts}`).

## Estructura d'un Active Object

- Rectangle blau, `verticalAlign=top`, títol en negreta: `Nom (QP::QActive)`
- Sub-rectangles verds a l'interior per a cada event d'entrada i sortida
- Format de l'etiqueta d'event: `SIGNAL_NAME, mecanisme\n(NomEvt)` — dues línies, `fontSize=8` (senyal i mecanisme a dalt, struct d'event entre parèntesis a baix; coherent amb la taula d'events del `CLAUDE.md`, que lidera amb el senyal)
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
