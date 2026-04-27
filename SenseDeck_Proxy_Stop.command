#!/bin/bash
PLIST=~/Library/LaunchAgents/com.cirutech.sensedeck-proxy.plist

if launchctl unload "$PLIST" 2>/dev/null; then
    echo "SenseDeck Proxy fermato."
else
    echo "SenseDeck Proxy non era in esecuzione."
fi
