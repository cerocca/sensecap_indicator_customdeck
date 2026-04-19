#!/bin/bash
cd "$(dirname "$0")"

PID_FILE="$(dirname "$0")/sensedeck_proxy.pid"

if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if kill -0 "$PID" 2>/dev/null; then
        kill "$PID"
        rm -f "$PID_FILE"
        echo "SenseDeck Proxy fermato (PID $PID)."
    else
        echo "Processo PID $PID non trovato, pulizia PID file."
        rm -f "$PID_FILE"
    fi
else
    # Fallback: pkill se il .pid non esiste
    if pkill -f sensedeck_proxy.py; then
        echo "SenseDeck Proxy fermato (via pkill)."
    else
        echo "Nessun processo SenseDeck Proxy in esecuzione."
    fi
fi
sleep 2
