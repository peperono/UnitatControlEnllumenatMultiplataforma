---
name: feedback_workflow
description: Flux de treball obligatori després de qualsevol modificació de codi
metadata: 
  node_type: memory
  type: feedback
  originSessionId: fd0c3492-b5f8-4acd-82ca-c93aa57f862b
---

Després de qualsevol modificació de codi (NO documentació), sempre en aquest ordre:
1. `bash build.sh` (Windows)
2. `source ~/esp/esp-idf/export.sh > /dev/null 2>&1 && idf.py build` (ESP32)
3. Si ambdues compilen: `git commit -am "..." && git push`

**Why:** Especificat explícitament a CLAUDE.md secció "Flux de treball". L'usuari va corregir perquè feia commit sense push.

**How to apply:** Sempre. Sense excepcions per a canvis de codi. El push és part obligatòria del flux, no opcional.
