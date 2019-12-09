#!/bin/sh
# Copyright (C) Sierra Wireless Inc.
#
# Mediatek Wifi PA for the mangOH Red platform
#
# ($1:) -d Debug logs
# $1: Command (ex:  WIFI_START
#                   WIFICLIENT_CONNECT
# $2: wpa_supplicant.conf file directory

if [ "$1" = "-d" ]; then
    shift
    set -x
fi

CMD=$1
# WiFi interface
IFACE=wlan0
# If WLAN interface exists but can not be brought up, means WiFi hardware is inserted,
# drivers are loaded successfully, but firmware failed to boot, tiwifi.sh returns 100
FIRMWAREFAILURE=100
# If WLAN interface does not exist but driver is installed, means WiFi hardware is absent
HARDWAREABSENCE=50
# WiFi driver is not installed
NODRIVER=100
TIMEOUT=8
# PATH
export PATH=/legato/systems/current/bin:/usr/local/bin:/usr/bin:/bin:/usr/local/sbin:/usr/sbin:/sbin

# Check the connection on the WiFi network interface.
# Exit with 0 if connected otherwise exit with 8 (time out)
CheckConnection()
{
    retries=10
    echo "Checking connection..."
    # Verify connection status
    for i in $(seq 1 ${retries})
    do
      echo "loop=${i}"
      (/usr/sbin/iw $1 link | grep "Connected to") && break
      sleep 1
    done
    if [ "${i}" -eq "${retries}" ]; then
        # Connection request time out.
        exit ${TIMEOUT}
    fi
    # Connected.
    exit 0
}

WiFiReset()
{
    retries=3
    echo "WiFi reset"
    for i in $(seq 1 ${retries})
    do
      sleep 1
      /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0
      sleep 1
      /legato/systems/current/modules/files/mt7697wifi_core/mtwifi start uart 0 && exit 0
    done
    exit 127
}

case ${CMD} in
  WIFI_START)
    echo "WIFI_START"
    # Create the WiFi network interface
    /legato/systems/current/modules/files/mt7697wifi_core/mtwifi start uart 0 && exit 0
    # Store failure reason
    FAILUREREASON=$?
    # If mtwifi indicates firmware fails to boot, do reset
    if [ ${FAILUREREASON} -eq ${FIRMWAREFAILURE} ]; then
        WiFiReset && exit 0
        # Reset fail, do clean up
        /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0
        exit ${FAILUREREASON}
    fi
    # Hardware is absent, do clean up
    if [ ${FAILUREREASON} -eq ${HARDWAREABSENCE} ]; then
        /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0
        exit ${FAILUREREASON}
    fi
    # Other reasons, do clean up also
    /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0
    exit ${FAILUREREASON} ;;

  WIFI_STOP)
    echo "WIFI_STOP"
    # Delete the WiFi network interface
    /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0 || exit 127
    exit 0 ;;

  WIFI_SET_EVENT)
    echo "WIFI_SET_EVENT"
    /usr/sbin/iw event || exit 127
    exit 0 ;;

  WIFI_UNSET_EVENT)
    echo "WIFI_UNSET_EVENT"
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] && exit 0
    for i in $(seq 1 ${count})
    do
      pid=$(/usr/bin/pgrep -n iw)
      /bin/kill -9 ${pid}
    done
    count=$(/usr/bin/pgrep -c iw)
    [ "${count}" -eq 0 ] || exit 127
    exit 0 ;;

  WIFI_CHECK_HWSTATUS)
    echo "WIFI_CHECK_HWSTATUS"
    #Client request disconnection if interface in up
    /sbin/ifconfig | grep ${IFACE} >/dev/null
    [ $? -eq 0 ] && exit 0
    sleep 1
    #Check WiFi stop called or not
    /sbin/lsmod | grep brcmfmac >/dev/null
    #Driver stays, hardware removed
    [ $? -eq 0 ] && exit ${HARDWAREABSENCE}
    #WiFi stop called
    exit ${NODRIVER} ;;

  WIFIAP_HOSTAPD_START)
    echo "WIFIAP_HOSTAPD_START"
    (/bin/hostapd /tmp/hostapd.conf -i${IFACE} -B) && exit 0
    # Fail to start hostapd, do cleanup
    /legato/systems/current/modules/files/mt7697wifi_core/mtwifi stop uart 0
    exit 127 ;;

  WIFIAP_HOSTAPD_STOP)
    echo "WIFIAP_HOSTAPD_STOP"
    rm -f /tmp/dnsmasq.wlan.conf
    touch /tmp/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    killall hostapd
    sleep 1;
    pidof hostapd && (kill -9 `pidof hostapd` || exit 127)
    pidof dnsmasq && (kill -9 `pidof dnsmasq` || exit 127)
    /etc/init.d/dnsmasq start || exit 127
    exit 0 ;;

  WIFIAP_WLAN_UP)
    echo "WIFIAP_WLAN_UP"
    AP_IP=$2
    /sbin/ifconfig | grep ${IFACE} || exit 127
    /sbin/ifconfig ${IFACE} ${AP_IP} up || exit 127
    exit 0 ;;

  DNSMASQ_RESTART)
    echo "interface=${IFACE}" >> /tmp/dnsmasq.wlan.conf
    /etc/init.d/dnsmasq stop
    pkill -9 dnsmasq
    /etc/init.d/dnsmasq start || exit 127
    exit 0 ;;

  WIFICLIENT_START_SCAN)
    echo "WIFICLIENT_START_SCAN"
    (/usr/sbin/iw dev ${IFACE} scan | grep 'BSS\|SSID\|signal') || exit 127
    exit 0 ;;

  WIFICLIENT_CONNECT)
    echo "WIFICLIENT_CONNECT"
    WPA_CFG=$2
    [ -f ${WPA_CFG} ] || exit 127
    # wpa_supplicant is running, return duplicated request
    /bin/ps -A | grep wpa_supplicant && exit 14

    # The Mediatek drivers were written to interface with the old wireless extensions.
    # Legato > 19.07 wants to properly upgrade to nl80211 which is the netlink-based userspace
    # interface for the new cfg80211 configuration system for wireless hardware
    #/sbin/wpa_supplicant -d -Dnl80211 -c ${WPA_CFG} -i${IFACE} -B || exit 127

    /sbin/wpa_supplicant -d -Dwext -c ${WPA_CFG} -i${IFACE} -B || exit 127
    CheckConnection ${IFACE} ;;

  WIFICLIENT_DISCONNECT)
    echo "WIFICLIENT_DISCONNECT"
    /sbin/wpa_cli -i${IFACE} terminate || exit 127
    echo "WiFi client disconnected."
    exit 0 ;;

  IPTABLE_DHCP_INSERT)
    echo "IPTABLE_DHCP_INSERT"
    /usr/sbin/iptables -I INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit 127
    exit 0 ;;

  IPTABLE_DHCP_DELETE)
    echo "IPTABLE_DHCP_DELETE"
    /usr/sbin/iptables -D INPUT -i ${IFACE} -p udp -m udp \
     --sport 67:68 --dport 67:68 -j ACCEPT  || exit 127
    exit 0 ;;

  *)
    echo "Parameter not valid"
    exit 127 ;;
esac

