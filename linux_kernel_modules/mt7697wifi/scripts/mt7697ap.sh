#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Interfaces configuration
export ITF_LAN="wlan1" # WiFi? access point interface
export ITF_WAN="rmnet0" # 3G/4G interface as WLAN interface

export APN="internet.sierrawireless.com"
export WIFIAPIP="192.168.20.1"
export WIFIAPMASK="255.255.255.0"
export WIFIAPSTART="192.168.20.10"
export WIFIAPSTOP="192.168.20.100"

# DHCP config file
export DHCP_CFG_FILE=dnsmasq.wlan.conf

echo "Mounting of $ITF_LAN..."
/sbin/ifup $ITF_LAN
ifconfig $ITF_LAN $WIFIAPIP netmask $WIFIAPMASK up

echo "Reconfiguring the DHCP server..."
/etc/init.d/dnsmasq stop || echo -ne ">>>>>>>>>>>>>>>>>>> UNABLE TO STOP THE DHCP server"
### Configure the IP addresses range for DHCP (dnsmasq)
test -L /etc/dnsmasq.d/$DHCP_CFG_FILE || ln -s /tmp/$DHCP_CFG_FILE /etc/dnsmasq.d/$DHCP_CFG_FILE

echo "Generating the configuration file for the DHCP server..."
echo -ne "log-dhcp\nlog-queries\n" \
         "log-facility=/tmp/dnsmasq.log\n" \
         "interface=$ITF_LAN\n" \
         "dhcp-option=$ITF_LAN,3,$WIFIAPIP\n" \
         "dhcp-option=$ITF_LAN,6,$WIFIAPIP\n" \
         "dhcp-range=$WIFIAPSTART,$WIFIAPSTOP,24h\n" \
	 "server=8.8.8.8\n" >> /tmp/$DHCP_CFG_FILE

echo "Mounting the relay interface $ITF_WAN..."
case "$ITF_WAN" in
    eth*)
        ifconfig $ITF_WAN up
        ;;
    rmnet* | ppp*)
        echo "Enabling the radio on $ITF_WAN..."
        cm radio on
        sleep 1
        echo "Enabling the data connection on $ITF_WAN..."
        cm data apn $APN
        cm data connect &
        sleep 1
        echo "Waiting for data connection on $ITF_WAN..."
        ;;
esac

### Start the DHCP server
echo "Restarting the DHCP server..."
/etc/init.d/dnsmasq start  || echo ">>>>>>>>>>>>>>>>>>> UNABLE TO START THE DHCP server"

echo "Check SIM inserted..."
cm sim

echo "Cleaning IP tables..."
iptables --flush
iptables --table nat --flush
iptables --table nat --delete-chain

echo "Configuring the NAT..."
iptables -P INPUT ACCEPT
iptables -P OUTPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables --table nat -A POSTROUTING -o $ITF_WAN -j MASQUERADE
iptables -A FORWARD -i $ITF_WAN -o $ITF_LAN -m state --state RELATED,ESTABLISHED -j ACCEPT
iptables -A FORWARD -i $ITF_LAN -o $ITF_WAN -m state --state NEW -j ACCEPT

echo "Enabling IP forwarding..."
sysctl -w net.ipv4.ip_forward=1

echo "Start wpa supplicant...."
/sbin/wpa_supplicant -Dwext -i $ITF_LAN -c /etc/mt7697-ap -B
