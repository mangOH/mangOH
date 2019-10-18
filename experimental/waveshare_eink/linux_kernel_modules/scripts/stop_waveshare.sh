#!/bin/sh
# The sleep's are below beacuse I am superstitious.

# Note any apps that have connected to /dev/fb need to exit
# before fb_waveshare_eink is removed - assumption is that
# Legato 19.07 removes all apps before modules. Note in
# Legato 19.07 any app dependencies on modules are removed
# in the wrong order - Jira ticket LE-13549 - add back
# proper dependencies in the apps once the ticket is fixed.
rmmod /legato/systems/current/modules/fb_waveshare_eink.ko
if [ $? -ne 0 ] ; then
	echo "rmmod fb_waveshare_eink failed" > /dev/console
	exit 1
fi
sleep 1

rmmod /legato/systems/current/modules/mangOH_yellow_ws213.ko
if [ $? -ne 0 ] ; then
	echo "rmmod mangOH_yellow_ws213 failed" > /dev/console
	exit 1
fi
sleep 1

VERSION=`uname -r`
for i in /lib/modules/${VERSION}/kernel/drivers/video/fbdev/core/*.ko ; do
	modprobe -r `basename $i .ko`
	if [ $? -ne 0 ] ; then
		echo "modprobe -r `basename $i .ko` failed" > /dev/console
		exit 1
	fi
done
