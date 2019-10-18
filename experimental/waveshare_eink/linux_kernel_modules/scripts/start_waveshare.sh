#!/bin/sh
# Eink kernel module installer - lets load any core FB loadable modules
# If the no core loadable modules exist you have a bogus kernel with no video
# helper routines.
# The sleep's below exist because we are superstitious.

# At times with Legato 19.07 & the associated SWI Linux/rootfs
# the sx1508 device has not been created when the mangOH 
# module is loaded. 

# Make 10 attempts to check whether sx1508 gpio expander on expander board
# has been created - RESET & DC GPIO are off the expander board expander
# Sleep 1s in between
for i in $(seq 1 10)
do
  if [ -d /sys/bus/i2c/devices/8-0021 ] ; then
    break
  fi
  sleep 1
done

# return error if sx1508 device file hasn't been created after 10 secs
if [ "$i" -eq "10" ] ; then
  echo "sx1508q gpiochip expander on expander board failed to be created" > /dev/console
  exit 1
fi


VERSION=`uname -r`

# Most displays will need at least some of the video/graphics helper routines.
if [ ! -d "/lib/modules/${VERSION}/kernel/drivers/video/fbdev/core" ] ; then
	echo "Kernel does not support video helper routines" > /dev/console
	exit 1
fi

for i in /lib/modules/${VERSION}/kernel/drivers/video/fbdev/core/*.ko ; do
	modprobe `basename $i .ko`
	if [ $? -ne 0 ] ; then
		echo "modprobe `basename $i .ko` failed" > /dev/console
		exit 1
	fi
done
sleep 1

# Let's load the Legato built kernel modules
insmod /legato/systems/current/modules/mangOH_yellow_ws213.ko
if [ $? -ne 0 ] ; then
	echo "insmod mangOH_yellow_ws213 failed" > /dev/console
	exit 1
fi
sleep 1

insmod /legato/systems/current/modules/fb_waveshare_eink.ko
if [ $? -ne 0 ] ; then
	echo "insmod fb_waveshare_eink failed" > /dev/console
	exit 1
fi
sleep 1

exit 0
