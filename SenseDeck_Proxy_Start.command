#!/bin/bash
cd "$(dirname "$0")"
nohup python3 -u sensedeck_proxy.py > sensedeck_proxy.log 2>&1 &
echo "SenseDeck Proxy avviato (PID $!)"
echo "Log: $(dirname "$0")/sensedeck_proxy.log"
sleep 2
