#!/bin/sh
# Broadcom/Cypress chip startup for mangOH Yellow

export PATH=$PATH:/usr/bin:/bin:/usr/sbin:/sbin

# The Broadcom/Cypress uses the WL_REG_ON gpio to bring out of reset, but
# needs 2 sleep cycles after VBAT & VDDIO is applied before it is
# turned on. Both the WP76 & 85 are setup to connect GPIO 33 to WL_REG_ON,
# though its configurable on the command-line. It seems with the current
# way that the GPIO expander is being initialized this is not needed, nor
# does the routing need to be setup for SDIO going to the Cypress chip
# rather than the SD card. The SD card was supposed to be the default.
#if  [ -z "$2" ] ; then
	#WL_REG_ON_GPIO=8
#else
	#WL_REG_ON_GPIO=$2
#fi

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
	# DV3 uses GPIO33 and DV4 uses wl_reg_on on the expander
	# It seems different DV3 boards were sent in the Yellow giveaway with different formats
	# So relax the search for the DV3 string
	head -n 2 /sys/bus/i2c/devices/4-0050/eeprom | grep -q DV3
	if [ $? -eq 0 ] ; then
		BOARD_REV="DV3"
		WL_REG_ON_GPIO=33
	fi
fi

if [ -z "${CF3}" ] ; then
	echo "Unknown CF3 Module"
	exit 1
fi

# Need the the init separately as the default cfg80211 from the Reference code 
# (kernel, yocto) loads a different cfg80211 than the Broadcom/Cypress cfg80211
# Thus, with Legato it causes the default one to load and the FMAC driver load
# fails.
cy_wifi_init () {
	insmod /legato/systems/current/modules/compat.ko
	insmod  /legato/systems/current/modules/cfg80211.ko
}
cy_wifi_start() {
	# Let's bring the Cypress chip out of reset
	if [ "${CF3}" = "mdm9x28" ] ; then
		# DV3 vs. DV4 diffs
          	if [ -n "$BOARD_REV" ] && [ ${BOARD_REV} == "DV3" ] ; then
                  	if [ ! -d "/sys/class/gpio/gpio${WL_REG_ON_GPIO}" ] ; then
				echo ${WL_REG_ON_GPIO} > /sys/class/gpio/export
				echo out  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/direction
			fi
                	WL_REG_ON_VALUE=`cat /sys/class/gpio/gpio${WL_REG_ON_GPIO}/value`
                        if [ $WL_REG_ON_VALUE -eq 0 ] ; then
				echo 1  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/value
			fi
		# Otherwise let's assume Rev 1.0 or greater
		else
			echo 1  > /sys/devices/platform/expander.0/wl_reg_on
          	fi            
		# TODO: fix. WL_HOST_WAKE (GPIO42) should tell us that the
		# the Cypress chip is powered on. On DV[34], may not be
		# connected. If it works the sleep below can be removed.
		sleep 2
		modprobe sdhci-msm
		# TODO: fix. Remove sleep below after more testing.
		# Thought I saw issues in the BRCM FMAC coming up to soon.
		# Btw, brcmfmac does have reconnect to sdio code.
		sleep 2
	fi

	if [ "${CF3}" = "mdm9x15" ] ; then
		rmmod msm_sdcc
		modprobe msm_sdcc
	fi

	# Let's make sure the Cypress chip got attached by the Kernel's MMC framework
	# Bug here if dmesg has overflowed - for now we are removing the code below
	# as we will assume MMC enumeration happened. Need a better way to error check
	# as the driver on the WP76 continuously polls CIS tuples ad-infinitum and messes up the log
	#sleep 3
        # TODO: Return exit code 50 here as the Chip was not scanned across SDIO
	dmesg | grep "new high speed SDIO card" > /dev/null 2>&1
	if [ $? -ne 0 ] ; then
		echo "Cypress chip MMC recognition may have been overwritten"
		#exit 2
	fi

	# We need to get this into the reference software or the product branch ASAP
	# and remove this temp. workaround, should be /lib/firmware - TODO
	if [ ! -d "/legato/systems/current/modules/files/brcmutil/etc/firmware/brcm" ]; then
  		echo "/legato/systems/current/modules/files/brcmutil/etc/firmware/brcm directory does not exist"
		exit 100
	fi

	# Currently, to load the broadcom driver we are changing the Kernel's FW search path
	echo -n /legato/systems/current/modules/files/brcmutil/etc/firmware > /sys/module/firmware_class/parameters/path

	# If you want all kernel messages on console
	#echo 8 > /proc/sys/kernel/printk

	# load the drivers
	insmod /legato/systems/current/modules/brcmutil.ko
	insmod /legato/systems/current/modules/brcmfmac.ko

	# The following bit vector tells the FMAC chip controller debug options
	#insmod ${load_module:-brcmfmac.ko} debug=0x100000
	#insmod ${load_module:-brcmfmac.ko} debug=0x105404
	

	# firmware loads and reboots the Cypress device
	sleep 5

	ifconfig -a | grep wlan0 >/dev/null
	if [ $? -ne 0 ] ; then
		echo "wlan0 interface was not created by Cypress drivers"; exit 100
        fi
	ifconfig wlan0 up >/dev/null
	if [ $? -ne 0 ] ; then
		echo "wlan0 interface UP not working; exit 127"
	fi
	# In case you have access to the Broadcom wl tool
	#./wlarmv7a mpc 0
	#./wlarmv7a mpc
}

cy_wifi_stop() {
	ifconfig | grep wlan0 >/dev/null
        # TODO FIX:  For now we have commented rmmod's and kicking the Cypress chip
	# The sdhci-msm crashes on modprobe -r 
	#if [ $? -eq 0 ]; then
	   #ifconfig wlan0 down
	#fi
	#rmmod /legato/systems/current/modules/brcmfmac.ko || exit 127
	#rmmod /legato/systems/current/modules/brcmutil.ko || exit 127
	#rmmod /legato/systems/current/modules/cfg80211.ko || exit 127
	#rmmod /legato/systems/current/modules/compat.ko || exit 127
	#modprobe -r sdhci-msm

	# Turn off the Cypress chip
	#echo 0  > /sys/class/gpio/gpio${WL_REG_ON_GPIO}/value
}

case "$1" in
	init)
		cy_wifi_init
		;;
	start)
		cy_wifi_init
		cy_wifi_start
		;;
	stop)
		cy_wifi_stop
		;;
	restart)
		cy_wifi_stop
		cy_wifi_init
		cy_wifi_start
		;;
	*)
		exit 99
		;;
esac

exit 0
