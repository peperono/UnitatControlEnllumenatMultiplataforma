#!/usr/bin/env bash
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT="$ROOT/index.html"
TMPFILE=$(mktemp)

for f in "$ROOT"/sub-*.html; do
  cat "$f" >> "$TMPFILE"
done

while IFS= read -r line; do
  if [[ "$line" == *'<!-- SUBSYSTEMS -->'* ]]; then
    cat "$TMPFILE"
  else
    echo "$line"
  fi
done < "$ROOT/nav.html" > "$OUT"

rm "$TMPFILE"
echo "Generat $OUT"
