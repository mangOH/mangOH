#!/bin/sh

echo "---------------------------------------------" > /dev/console

KO_PATH=$1

rmmod $KO_PATH
modprobe -rq spidev

exit 0
