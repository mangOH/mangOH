#!/bin/sh
# Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
#

cd /etc/bme680/
insmod 3-bme680-i2c.ko
insmod 4-bmi088-i2c.ko
insmod 9-mangoh_yellow_dv1.ko

echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_x_en
echo "ACCEL-X enable"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_x_en

echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_y_en
echo "ACCEL-Y enable"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_y_en

echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_z_en
echo "ACCEL-Z enable"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_z_en

echo 0 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_nomotion_sel
echo "ACCEL no motion selected"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_nomotion_sel

echo 100 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_duration
echo "ACCEL duration"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_duration

echo 100 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_threshold
echo "ACCEL threshold"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_threshold

echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/scan_elements/in_anglvel_x_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/scan_elements/in_anglvel_y_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/scan_elements/in_anglvel_z_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/scan_elements/in_timestamp_en
echo 5 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/buffer/length
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/buffer/enable

echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/scan_elements/in_accel_x_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/scan_elements/in_accel_y_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/scan_elements/in_accel_z_en
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/scan_elements/in_timestamp_en
echo 5 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/buffer/length
echo 1 > /sys/devices/78b8000.i2c/i2c-4/4-0018/iio\:device1/buffer/enable

echo "TEMP"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_temp_input
echo "ACCEL-X"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_x_input
echo "ACCEL-Y"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_y_input
echo "ACCEL-Z"
cat /sys/devices/78b8000.i2c/i2c-4/4-0018/iio:device1/in_accel_z_input

echo "ANG VELOCITY-X"
cat /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/in_anglvel_x_input
echo "ANG VELOCITY-Y"
cat /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/in_anglvel_y_input
echo "ANG VELOCITY-Z"
cat /sys/devices/78b8000.i2c/i2c-4/4-0068/iio\:device2/in_anglvel_z_input

cat /dev/iio\:device1
