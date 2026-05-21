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
  "dilluns":   [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "dimarts":   [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "dimecres":  [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "dijous":    [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "divendres": [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "dissabte":  [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}],
  "diumenge":  [{"id":1,"act":"on","time":"01:00"},{"id":1,"act":"off","time":"02:00"},{"id":2,"act":"on","time":"02:00"},{"id":2,"act":"off","time":"03:00"},{"id":3,"act":"on","time":"03:00"},{"id":3,"act":"off","time":"04:00"}]
}
)RAWJSON";
