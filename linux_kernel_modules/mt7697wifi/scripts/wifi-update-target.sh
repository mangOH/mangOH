#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

echo "Copying setup MT7697 WiFi script to target..."
scp $MANGOH_ROOT/linux_kernel_modules/mt7697wifi/scripts/mt7697_setup_wifi.sh root@192.168.2.2:/tmp/.

echo "Copying network interfaces to target..."
scp $MANGOH_ROOT/linux_kernel_modules/mt7697wifi/scripts/interfaces root@192.168.2.2:/tmp/.

echo "Copying MT7697 ifup/ifdown script to target..."
scp $MANGOH_ROOT/linux_kernel_modules/mt7697wifi/scripts/mtwifi root@192.168.2.2:/tmp/.

