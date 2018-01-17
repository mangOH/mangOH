#!/bin/sh
# Copyright (C) Sierra Wireless Inc.
#

export ITF_LAN="wlan1" # WiFi? access point interface
export CLIENT_REF="0x10000001"
export PASSPHRASE="@iPr1m3"
export SECURITY_PROTO="3"

echo "Start wifi client..."
wifi client start $ITF_LAN

echo "Begin wifi client scan..."
wifi client scan $ITF_LAN

echo "Set security parameters..."
wifi client setsecurityproto $CLIENT_REF $SECURITY_PROTO
wifi client setpassphrase $CLIENT_REF $PASSPHRASE

echo "Set WPA driver..."
wifi client setdriver $CLIENT_REF 1

echo "Connect to access point..."
wifi client connect $CLIENT_REF
