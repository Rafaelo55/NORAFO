#!/usr/bin/env bash
# Uso: Tools/run_test.sh <circuito> <modo> [extra...]
# Ejecuta el juego (build de editor en modo -game) con una prueba automatica y muestra el resumen.
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
C="$1"; M="$2"; shift 2
URL="L_City?Circuit=$C"
[ "$C" = "menu" ] && URL="L_City"
MSYS_NO_PATHCONV=1 timeout 900 "/c/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe" "$ROOT/NoSleepPogo.uproject" "$URL" \
  -game -windowed -ResX=1280 -ResY=720 -PogoTest=$M -unattended -nosplash -log -PogoUnlockAll "$@" > /dev/null 2>&1
LOG="$ROOT/Saved/Logs/NoSleepPogo.log"
cp "$LOG" "$ROOT/Saved/PogoTest/log_c${C}_${M}.txt" 2>/dev/null
grep -E "POGO_TEST|LogPogo: (META|Caida|Circuito)|Error:.*Pogo" "$LOG" | sed -E 's/^\[[^]]*\]\[[^]]*\]//'
