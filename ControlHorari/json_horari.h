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
  "dilluns":   [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "dimarts":   [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "dimecres":  [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "dijous":    [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "divendres": [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "dissabte":  [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}],
  "diumenge":  [{"id":1,"act":"on","time":"21:00"},{"id":1,"act":"off","time":"06:00"}]
}
)RAWJSON";
