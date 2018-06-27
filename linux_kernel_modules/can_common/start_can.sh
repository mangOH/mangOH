# It seems that taking the IoT card out of reset fails in the core
# mangOH driver - but placing things after user-space has fully started
# and through the SWI gpiolib-sysfs code works - TODO fix and move into
# the core driver so no shell script is needed.
# Thus, we remove the inserted chip driver and bring it back.

drv_file=`find /legato/systems/current/modules/ -name "*mcp251x.ko"`
drv=`basename $drv_file`
# remove the driver
rmmod $drv

# Take IoT card out of reset
echo 2 > /sys/class/gpio/export
echo out  > /sys/class/gpio/gpio2/direction
echo 1  > /sys/class/gpio/gpio2/value

# Enable level shifter on the CAN IoT card
echo 13 > /sys/class/gpio/export
echo out  > /sys/class/gpio/gpio13/direction
echo 1  > /sys/class/gpio/gpio13/value

# Bring driver back & iproute2 add in CAN
insmod $drv_file
ip link set can0 type can bitrate 125000 triple-sampling on
ifconfig can0 up
