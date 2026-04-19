#!/bin/bash
cd "$(dirname "$0")"

# Kill eventuale istanza precedente
EXISTING=$(pgrep -f sensedeck_proxy.py)
if [ -n "$EXISTING" ]; then
    echo "Processo precedente trovato (PID $EXISTING), lo termino..."
    kill $EXISTING
    sleep 1
fi

# Avvio con nohup, log append per non perdere la sessione precedente
nohup python3 -u sensedeck_proxy.py >> sensedeck_proxy.log 2>&1 &
echo $! > "$(dirname "$0")/sensedeck_proxy.pid"
echo "SenseDeck Proxy avviato (PID $!)"
echo "Log: $(dirname "$0")/sensedeck_proxy.log"
sleep 2
