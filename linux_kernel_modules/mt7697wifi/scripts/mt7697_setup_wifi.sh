#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

echo "Update network configuration for MT7697 WiFi..."

if [ -f "/etc/network/interfaces" ]
then
    export NET_INTERFACES_BACKUP_FILE=/etc/network/interfaces$(date +"%Y%m%dT%H%M").bak
    echo "Backing up current network interfaces -> $NET_INTERFACES_BACKUP_FILE..."
    mv /etc/network/interfaces $NET_INTERFACES_BACKUP_FILE
fi

echo "Updating network interfaces..."
cp /tmp/interfaces /etc/network/interfaces

if [ -f "/etc/init.d/mtwifi" ]
then
    export WIFI_INIT_BACKUP_FILE=/etc/init.d/mtwifi$(date +"%Y%m%dT%H%M").bak
    echo "Backing up current MT7697 WiFi init script -> $WIFI_INIT_BACKUP_FILE..."
    mv /etc/init.d/mtwifi $WIFI_INIT_BACKUP_FILE
fi

echo "Updating MT7697 WiFi init script..."
cp /tmp/mtwifi /etc/init.d/mtwifi
chmod +x /etc/init.d/mtwifi
