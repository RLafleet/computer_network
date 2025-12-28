#!/usr/bin/env bash
set -euo pipefail

OUT_FILE="${1:-demo_1mb.txt}"
SIZE_BYTES="${2:-1048576}"

if [[ -f "$OUT_FILE" ]]; then
  echo "Файл уже существует: $OUT_FILE"
  exit 0
fi

yes "RDTP demo line 0123456789" | head -c "$SIZE_BYTES" > "$OUT_FILE"

echo "Готово: $OUTч_FILE ($SIZE_BYTES байт)"
