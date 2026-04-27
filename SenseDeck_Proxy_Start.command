#!/bin/bash
PLIST=~/Library/LaunchAgents/com.cirutech.sensedeck-proxy.plist

# Unload first in case it's already loaded (avoids "already loaded" error)
launchctl unload "$PLIST" 2>/dev/null

launchctl load "$PLIST"
echo "SenseDeck Proxy avviato via launchd."
echo "Log: /Users/ciru/sensecap_indicator_cirutech/sensedeck_proxy.log"
