#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Interfaces configuration
export ITF_LAN="eth0" # LAN interface
export ITF_WAN="rmnet_data0" # 3G/4G interface as WLAN interface

export APN="internet.sierrawireless.com"
export LAN_IP="192.168.20.1"
export NET_MASK="255.255.255.0"
export DHCP_IP_START="192.168.20.10"
export DHCP_IP_END="192.168.20.100"
export WPA_SUPPLICANT_CFG_FILE="/etc/wpa_supplicant.cfg"

# DHCP config file
export DHCP_CFG_FILE_PREFIX="dnsmasq"
export DHCP_CFG_FILE_SUFFIX="conf"

# Test an IP address for validity:
valid_ip()
{
    local ip=$1

    echo "IP address ($ip)"
    local result=$(expr "$ip" : '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$')
    if [ $result -gt 0 ]; then
        IFS=.
        set $ip
        for quad in 1 2 3 4; do
            if eval [ \$$quad -gt 255 ]; then
                echo "Error IP address out of range($ip)"
                return 1
            fi
        done

        return 0
    else
        echo "Error invalid IP address ($ip)"
        return 1
    fi
}

# Test an Net Mask for validity:
valid_net_mask()
{
    local net_mask=$1
    local zero=0

    echo "net mask ($net_mask)"
    local result=$(expr "$net_mask" : '[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$')
    if [ $result -gt 0 ]; then
        IFS=.
        set $net_mask
        for quad in 1 2 3 4; do
            if eval [ \$$quad -lt 255 ]; then
                if eval [ \$$quad -ne 254 -a \
                          \$$quad -ne 252 -a \
                          \$$quad -ne 248 -a \
                          \$$quad -ne 240 -a \
                          \$$quad -ne 224 -a \
                          \$$quad -ne 192 -a \
                          \$$quad -ne 128 -a \
                          \$$quad -ne 0 ]; then
                    echo "Error net mask out of range ($net_mask)"
                    return 1
                else
                    zero=1
                fi
            else
                if eval [ $zero -ne 0 ]; then
                    echo "Error net mask out of range ($net_mask)"
                    return 1
                fi
            fi
        done

        return 0
    else
        echo "Error invalid net mask ($net_mask)"
        return 1
    fi
}

echo "MangOH gateway configuration"

read -p "Enter WAN (cellular) interface (default: rmnet_data0): " ITF_WAN
if [ -n $ITF_WAN ]; then
    ITF_WAN="rmnet_data0"
fi

read -p "Enter LAN interface (default: eth0): " ITF_LAN
if [ -n $ITF_LAN ]; then
    ITF_LAN="eth0"
fi

while true
do
    read -p "Enter LAN IP (default: 192.168.20.1): " LAN_IP
    if [ -n $LAN_IP ]; then
        LAN_IP="192.168.20.1"
    fi

    valid_ip "$LAN_IP"
    if [ "$?" -eq 0 ]; then
        break
    fi
done

while true
do
    read -p "Enter LAN IP Mask (default: 255.255.255.0): " NET_MASK
    if [ -n $NET_MASK ]; then
        NET_MASK="255.255.255.0"
    fi

    valid_net_mask "$NET_MASK"
    if [ "$?" -eq 0 ]; then
        break
    fi
done

if [[ ! -d /sys/class/net/${ITF_LAN} ]]; then
    echo "Bring up LAN interface $ITF_LAN"
    /sbin/ifup $ITF_LAN
    if [ "$?" -ne 0 ]; then
        echo "Bring up LAN interface $ITF_LAN failed"
        exit 1
    fi
fi

echo "Mounting LAN interface '$ITF_LAN' IP: '$LAN_IP', net mask: '$NET_MASK'"
/sbin/ifconfig "$ITF_LAN" "$LAN_IP" netmask "$NET_MASK" up
if [ "$?" -ne 0 ]; then
    echo "Mounting LAN interface '$ITF_LAN' IP: '$LAN_IP', net mask: '$NET_MASK' failed"
    exit 1
fi

echo "Check SIM inserted..."
cm sim

echo "Mounting the relay interface '$ITF_WAN'..."
case "$ITF_WAN" in
    eth*)
        /sbin/ifconfig "$ITF_WAN" up
        ;;
    rmnet* | ppp*)
        echo "Enabling the radio on '$ITF_WAN'..."
        cm radio on
        sleep 1

        read -p "Enter APN (default: internet.sierrawireless.com): " APN
        if [ -n $APN ]; then
            APN="internet.sierrawireless.com"
        fi

        echo "Enabling the data connection on '$ITF_WAN' using APN '$APN'..."
        cm data apn "$APN"
        cm data pdp ipv4
        cm data connect &
        sleep 1
        echo "Waiting for data connection on '$ITF_WAN'..."
        ;;
esac

RETRY=0
while [ ${RETRY} -lt 60 ] ; do
    ITF_WAN_ADDR=$(/sbin/ifconfig "$ITF_WAN" | grep "inet addr" | cut -d ':' -f 2 | cut -d ' ' -f 1)
    if [ "${ITF_WAN_ADDR}" == "" ]; then
        sleep 1
        RETRY=$((${RETRY} + 1))
    else
        break
    fi
done

if [ "${ITF_WAN_ADDR}" == "" ]; then
    echo "Mounting of relay interface '${ITF_WAN}' failed"
    exit 1
else
    echo "Relay interface '${ITF_WAN}' IP address is '${ITF_WAN_ADDR}'"
    route add default gw "$ITF_WAN_ADDR" "$ITF_WAN"
    sleep 5
fi

echo "Cleaning IP tables..."
iptables --flush
if [ "$?" -ne 0 ]; then
    echo "Flush iptables failed"
    exit 1
fi
iptables --table nat --flush
if [ "$?" -ne 0 ]; then
    echo "Flush nat iptables failed"
    exit 1
fi
iptables --table nat --delete-chain
if [ "$?" -ne 0 ]; then
    echo "Flush nat iptables delete-chain failed"
    exit 1
fi

echo "Enabling IP forwarding..."
sysctl -w net.ipv4.ip_forward=1

echo "Configuring the NAT..."
iptables -P INPUT ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables accept input failed"
    exit 1
fi
iptables -P OUTPUT ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables accept output failed"
    exit 1
fi
iptables -P FORWARD ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables accept forward failed"
    exit 1
fi
iptables --table nat -A POSTROUTING -o $ITF_WAN -j MASQUERADE
if [ "$?" -ne 0 ]; then
    echo "iptables postrouting failed"
    exit 1
fi
iptables -A FORWARD -i $ITF_WAN -o $ITF_LAN -m state --state RELATED,ESTABLISHED -j ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables forward failed"
    exit 1
fi
iptables -A FORWARD -i $ITF_LAN -o $ITF_WAN -m state --state NEW -j ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables forward failed"
    exit 1
fi
iptables -A INPUT -m udp -p udp --sport 67:68 --dport 67:68 -j ACCEPT
if [ "$?" -ne 0 ]; then
    echo "iptables accept input failed"
    exit 1
fi

while true
do
    read -p "Configure DHCP server (Y/n): " rsp
    if [ "${rsp}" = 'Y' -o "${rsp}" = 'n' ]; then
        break
    fi
done

if [ "${rsp}" = 'Y' ]; then
    while true
    do
        read -p "Enter start IP (default: 192.168.20.10): " DHCP_IP_START
        if [ -n $DHCP_IP_START ]; then
            DHCP_IP_START="192.168.20.10"
        fi

        valid_ip "$DHCP_IP_START"
        if [ "$?" -eq 0 ]; then
            break
        fi
    done

    while true
    do
        read -p "Enter end IP (default: 192.168.20.100): " DHCP_IP_END
        if [ -n $DHCP_IP_END ]; then
            DHCP_IP_END="192.168.20.100"
        fi

        valid_ip "$DHCP_IP_END"
        if [ "$?" -eq 0 ]; then
            break
        fi
    done

    echo "Reconfiguring the DHCP server..."
    /etc/init.d/dnsmasq stop || echo "UNABLE TO STOP THE DHCP server"
    ### Configure the IP addresses range for DHCP (dnsmasq)
    DHCP_CFG_FILE="$DHCP_CFG_FILE_PREFIX.$ITF_LAN.$DHCP_CFG_FILE_SUFFIX"
    test -L "/etc/dnsmasq.d/$DHCP_CFG_FILE" && rm -f "/etc/dnsmasq.d/$DHCP_CFG_FILE"
    test -f "/tmp/$DHCP_CFG_FILE" && rm -f "/tmp/$DHCP_CFG_FILE"
    #test -L "/etc/dnsmasq.d/$DHCP_CFG_FILE" || ln -s "/tmp/$DHCP_CFG_FILE" "/etc/dnsmasq.d/$DHCP_CFG_FILE"

    echo "Generating the configuration file '/tmp/$DHCP_CFG_FILE' for the DHCP server..."
    echo -ne "log-dhcp\nlog-queries\n" \
             "log-facility=/tmp/dnsmasq.log\n" \
             "interface=$ITF_LAN\n" \
             "dhcp-option=$ITF_LAN,3,$LAN_IP\n" \
             "dhcp-option=$ITF_LAN,6,$LAN_IP\n" \
             "dhcp-range=$DHCP_IP_START,$DHCP_IP_END,24h\n" \
	     "server=8.8.8.8\n" >> "/tmp/$DHCP_CFG_FILE"

    ln -s "/tmp/$DHCP_CFG_FILE" "/etc/dnsmasq.d/$DHCP_CFG_FILE"

    ### Start the DHCP server
    echo "Restarting the DHCP server..."
    /etc/init.d/dnsmasq start || echo "UNABLE TO START THE DHCP server"
fi

echo "MangOH gateway configuration completed"
