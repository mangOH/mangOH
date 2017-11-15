#!/bin/sh
# Copyright (C) Sierra Wireless Inc.
#

export ITF_LAN="wlan1" # WiFi? access point interface

echo "Start wifi web application..." 
app start wifiWebAp $ITF_LAN

