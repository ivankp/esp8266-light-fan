#!/usr/bin/env bash

SSID='light-and-fan'

nmcli device wifi rescan ssid "$SSID"

nmcli device wifi connect "$SSID"

nmcli dev status \
| awk ''' {
  if ($2 == "wifi") {
    print $0
    if ($4 == "'''"$SSID"'''" && $3 == "connected") {
      e = 1
    }
    exit
  }
} END { exit !e }'''

if [ $? -ne 0 ]; then
  echo "Failed to connect to $SSID"
  exit 1
fi
