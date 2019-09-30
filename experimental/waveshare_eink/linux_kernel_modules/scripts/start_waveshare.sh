#!/bin/sh
# Eink kernel module installer - lets load any core FB loadable modules
# If the no core loadable modules exist you have a bogus kernel with no video
# helper routines.
# The sleep's below exist because we are superstitious.

# At times with Legato 19.07 & the associated SWI Linux/rootfs
# the SPI busy port has not been created when the mangOH config
# module is loaded. 

# Make 10 attempts to check whether SPI busy port for Eink exists
# Sleep 1s in between
for i in $(seq 1 10)
do
  if [ -d /sys/devices/78b8000.i2c/i2c-4/i2c-8/8-003e ] ; then
    break
  fi
  sleep 1
done

# return error if device file hasn't been created after 10 secs
if [ "$i" -eq "10" ] ; then
  echo "SPI busy existence via the sx1509q gpiochip failed" > /dev/console
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
