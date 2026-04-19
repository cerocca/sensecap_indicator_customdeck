#!/bin/bash
cd "$(dirname "$0")"

EXISTING=$(pgrep -f sensedeck_proxy.py)
if [ -n "$EXISTING" ]; then
    echo "Processo precedente trovato (PID $EXISTING), lo termino..."
    kill $EXISTING
    sleep 1
fi

echo "SenseDeck Proxy avviato con auto-restart wrapper"
echo "Log: $(pwd)/sensedeck_proxy.log"

PROXY_DIR="$(pwd)"
nohup bash -c "
  cd \"$PROXY_DIR\"
  while true; do
    python3 -u sensedeck_proxy.py >> sensedeck_proxy.log 2>&1
    EXIT_CODE=\$?
    if [ \$EXIT_CODE -eq 42 ]; then
      echo \"[wrapper] proxy uscito con codice 42 — restart in 3s\" >> sensedeck_proxy.log
      sleep 3
    else
      echo \"[wrapper] proxy uscito con codice \$EXIT_CODE — stop\" >> sensedeck_proxy.log
      break
    fi
  done
" &

echo $! > sensedeck_proxy.pid
echo "Wrapper PID: $!"
