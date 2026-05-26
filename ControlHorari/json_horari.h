// Programació horària per defecte, carregada a l'inici.
// Editable en temps d'execució via POST /programacio_horaria.
//
// Format:
// {
//   "<dia>": [
//     { "id": <int>,   // ID de la sortida (S1, S2, ...)
//       "act": "on"|"off",
//       "time": "HH:MM"
//     }, ...
//   ], ...
// }
// Dies vàlids: dilluns, dimarts, dimecres, dijous, divendres, dissabte, diumenge

static const char JSON_HORARI[] = R"RAWJSON(
{
  "dilluns":   [],
  "dimarts":   [],
  "dimecres":  [],
  "dijous":    [],
  "divendres": [],
  "dissabte":  [],
  "diumenge":  []
}
)RAWJSON";
