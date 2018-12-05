#!/bin/sh
# Broadcom/Cypress chip startup for MangOH Yellow

export PATH=$PATH:/usr/bin:/bin:/usr/sbin:/sbin

# The Broadcom/Cypress uses the WL_REG_ON gpio to bring out of reset, but
# needs 2 sleep cycles after VBAT & VDDIO is applied before it is
# turned on. Both the WP76 & 85 are setup to connect GPIO 8 to WL_REG_ON,
# though its configurable on the command-line
if  [ -z "$2" ] ; then
	WL_REG_ON_GPIO=8
else
	WL_REG_ON_GPIO=$2
fi

# The WP76 series has an SDHCI-MSM driver that has been setup to run in polled
# mode. Whereas, the WP85 has an MSM-SDCC driver that has been setup to run in
# suspend mode. Thus, we need to kick-start the MSM-SDCC controller on the WP85
# to rescan the bus, but not on the WP76.
uname -a | grep mdm9x15
if [ $? -eq 0 ] ; then
	CF3="mdm9x15"
fi

uname -a | grep mdm9x28
if [ $? -eq 0 ] ; then
	CF3="mdm9x28"
fi

if [ -z "${CF3}" ] ; then
	echo "Unknown CF3 Module"
	exit 1
fi

cy_wifi_start() {
	# Let's bring the Cypress chip out of reset
	echo ${WL_REG_ON_GPIO} > /sys/class/gpio/export
	echo out  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/direction
	echo 1  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/value

	if [ "${CF3}" = "mdm9x15" ] ; then
		rmmod msm_sdcc
		modprobe msm_sdcc
	fi

	# Let's make sure the Cypress chip got attached by the Kernel's MMC framework
	# Bug here if dmesg has overflowed - for now we are removing the code below
	# as we will assume MMC enumeration happened. Need a better way to error check
	# as the driver on the WP76 continuously polls CIS tuples ad-infintum and messes up the log
	#sleep 3
	#dmesg | grep "new high speed SDIO card"
	#if [ $? -ne 0 ] ; then
		#echo "Cypress chip not recognized across MMC/SDIO bus"
		#exit 2
	#fi

	# We need to get this into the reference software or the product branch ASAP
	# and remove this temp. workaround, should be /lib/firmware - TODO
	if [ ! -d "/legato/systems/current/modules/files/brcmutil/etc/firmware/brcm" ]; then
  		echo "/legato/systems/current/modules/files/brcmutil/etc/firmware/brcm directory does not exist"
		exit 10
	fi

	# Currently, to load the broadcom driver we are changing the Kernel's FW search path
	echo -n /legato/systems/current/modules/files/brcmutil/etc/firmware > /sys/module/firmware_class/parameters/path

	# If you want all kernel messages on console
	#echo 8 > /proc/sys/kernel/printk

	# load the drivers
	insmod `find /legato/systems/current/modules/ -name "*compat.ko"`
	insmod  `find /legato/systems/current/modules/ -name "*cfg80211.ko"`
	insmod `find /legato/systems/current/modules/ -name "*brcmutil.ko"`

	insmod `find /legato/systems/current/modules/ -name "*brcmfmac.ko"`
	# The following bit vector tells the FMAC chip controller debug options
	#insmod ${load_module:-brcmfmac.ko} debug=0x100000
	#insmod ${load_module:-brcmfmac.ko} debug=0x105404
	

	# firmware loads and reboots the Cypress device
	sleep 5

	ifconfig -a | grep wlan1 >/dev/null
	if [ $? -ne 0 ] ; then
		echo "wlan1 interface was not created by Cypress drivers"; exit 127
	fi
	ifconfig wlan1 up >/dev/null
	if [ $? -ne 0 ] ; then
		echo "wlan1 interface UP not working; exit 127"
	fi
	# In case you have access to the Broadcom wl tool
	#./wlarmv7a mpc 0
	#./wlarmv7a mpc
}

cy_wifi_stop() {
	ifconfig | grep wlan1 >/dev/null
	if [ $? -eq 0 ]; then
	   ifconfig wlan1 down
	fi
	rmmod `find /legato/systems/current/modules/ -name "*brcmfmac.ko"` || exit 127
	rmmod `find /legato/systems/current/modules/ -name "*brcmutil.ko"` || exit 127
	rmmod `find /legato/systems/current/modules/ -name "*cfg80211.ko"` || exit 127
	rmmod `find /legato/systems/current/modules/ -name "*compat.ko"` || exit 127
	# Turn off the Cypress chip
	echo 0  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/value
}

case "$1" in
	start)
		cy_wifi_start
		;;
	stop)
		cy_wifi_stop
		;;
	restart)
		cy_wifi_stop
		cy_wifi_start
		;;
	*)
		exit 99
		;;
esac

exit 0
