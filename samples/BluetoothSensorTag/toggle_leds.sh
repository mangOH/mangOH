#!/bin/sh

while true; do
    dhub push /app/bluetoothSensorTag/io/value --json '{ "redLed": true, "greenLed": false, "buzzer": false }';
    sleep 2;
    dhub push /app/bluetoothSensorTag/io/value --json '{ "redLed": false, "greenLed": true, "buzzer": false }';
    sleep 2;
done
