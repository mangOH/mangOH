#!/bin/sh

# It seems that taking the IoT card out of reset fails in the core
# mangOH driver - but placing things after user-space has fully started
# and through the SWI gpiolib-sysfs code works - TODO fix and move into
# the core driver so no shell script is needed.
# Thus, we remove the inserted chip driver and bring it back.

usage () {
    echo "\"$0 board [slot]\" where board is \"green\" or \"red\" and slot is \"0\" or \"1\""
}

if [ "$#" -lt 1 ]; then
    usage
    exit 1
fi

if [ "$#" -gt 2 ]; then
    usage
    exit 1
fi

drv="can_iot.ko"
# remove the driver
kmod unload $drv

board=$1
if [ "$board" = "green" ]; then
    slot=0
    if [ "$#" -eq 2 ]; then
	slot=$2
    fi

    if [ "$slot" -eq 0 ]; then
	# Output 1 to IoT slot 0 GPIO2
	echo 33 > /sys/class/gpio/export
	echo out > /sys/class/gpio/gpio33/direction
	echo 1 > /sys/class/gpio/gpio33/value
	# Route SPI to IoT slot 0
	mux 4
	# Reset IoT slot 0
	mux 15
    elif [ "$slot" -eq 1 ]; then
	# Output 1 to IoT slot 0 GPIO2
	echo 7 > /sys/class/gpio/export
	echo out > /sys/class/gpio/gpio7/direction
	echo 1 > /sys/class/gpio/gpio7/value
	# Route SPI to IoT slot 1
	mux 5
	# Reset IoT slot 1
	mux 16
    else
	echo "Invalid or unsupported slot $slot"
	exit 1
    fi

elif [ "$board" = "red" ]; then
    # Enable level shifter on the CAN IoT card by driving IoT GPIO2 high
    echo 13 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio13/direction
    echo 1  > /sys/class/gpio/gpio13/value

    # Take IoT card out of reset
    echo 2 > /sys/class/gpio/export
    echo out > /sys/class/gpio/gpio2/direction
    echo 1  > /sys/class/gpio/gpio2/value

    # Give a bit of time for the IoT card to come out of reset before loading drivers
    sleep 1
fi


# Bring driver back & iproute2 add in CAN
kmod load $drv
ip link set can0 type can bitrate 125000 triple-sampling on
ifconfig can0 up
