#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Interfaces configuration
export ITF_LAN="wlan1" # WiFi? access point interface
export ITF_WAN="rmnet0" # 3G/4G interface as WLAN interface

export WIFIAPIP="192.168.20.1"
export WIFIAPMASK="255.255.255.0"
export SUBNET="192.168.0.0/24"
export WIFIAPSTART="192.168.20.10"
export WIFIAPSTOP="192.168.20.100"

# DHCP config file
export DHCP_CFG_FILE=dnsmasq.wlan.conf

/sbin/ifup wlan1
echo "Mounting of $ITF_LAN..."
ifconfig $ITF_LAN $WIFIAPIP netmask $WIFIAPMASK up

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
        cm data apn sp.telus.com
        cm data connect &
        sleep 1
        echo "Waiting for data connection on $ITF_WAN..."
        ;;
esac
 
RETRY=0
while [ $RETRY -lt 30 ] ; do
    ITF_WAN_ADDR=$(ifconfig $ITF_WAN | grep "inet addr" | cut -d ':' -f 2 | cut -d ' ' -f 1)
    if [ "$ITF_WAN_ADDR" == "" ]; then
        sleep 1
        RETRY=$(($RETRY + 1))
    else
        break
    fi
done

if [ "$ITF_WAN_ADDR" == "" ]; then
    echo "Mounting of relay interface $ITF_WAN failed..."
else
    echo "Relay interface $ITF_WAN IP address is $ITF_WAN_ADDR"
    route add default gw $ITF_WAN_ADDR $ITF_WAN
fi

echo "Enabling IP forwarding..."
echo 1 > /proc/sys/net/ipv4/ip_forward
echo "Configuring the NAT..."
modprobe ipt_MASQUERADE
iptables -A POSTROUTING -t nat -o $ITF_WAN -j MASQUERADE
iptables -A FORWARD --match state --state RELATED,ESTABLISHED --jump ACCEPT
iptables -A FORWARD -i $ITF_LAN --destination $SUBNET --match state --state NEW --jump ACCEPT
iptables -A INPUT -s $SUBNET --jump ACCEPT

echo "Moving IP tables rules..."
if [ ! -d /etc/iptables/backup ]; then
    mkdir /etc/iptables/backup
fi
if [ ! -f /etc/iptables/rules.v4 ]; then
    mv /etc/iptables/rules.v4 /etc/iptables/backup/.
fi
if [ ! -f /etc/iptables/rules.v4 ]; then
    mv /etc/iptables/rules.v6 /etc/iptables/backup/.
fi

echo "Reconfiguring the DHCP server..."
/etc/init.d/dnsmasq stop || echo -ne ">>>>>>>>>>>>>>>>>>> UNABLE TO STOP THE DHCP server"
### Configure the IP addresses range for DHCP (dnsmasq)
test -L /etc/dnsmasq.d/$DHCP_CFG_FILE || ln -s /tmp/$DHCP_CFG_FILE /etc/dnsmasq.d/$DHCP_CFG_FILE

echo "Generating the configuration file for the DHCP server..."
echo -ne "log-dhcp\nlog-queries\nlog-facility=/tmp/dnsmasq.log\ndhcp-range=$ITF_LAN,$WIFIAPSTART,$WIFIAPSTOP,24h\nserver=8.8.8.8\n" >> /tmp/$DHCP_CFG_FILE
### Start the DHCP server
echo "Restarting the DHCP server..."
/etc/init.d/dnsmasq start  || echo ">>>>>>>>>>>>>>>>>>> UNABLE TO START THE DHCP server"

echo "Start wpa supplicant...."
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/mnt/flash/wifi
/mnt/flash/mtwifi/wpa_supplicant -Dwext -i wlan1 -c /etc/mt7697-ap -B
