# BluetoothSensorTag

This is a Legato app which was written to serve as an example of how you can interact with a
Bluetooth device using Bluez.

## Special Setup Required
On the target, open `/etc/smack/accesses` and add the following:
```
_ app.bluetoothSensorTag -w----
app.bluetoothSensorTag _ -w----
```
Adding the above lines allows the app running as smack label "app.bluetoothSensorTag" to communicate
with dbus running as label "_". This is not a great security practice since many processes run with
smack label "_". Ideally, dbus would run with smack label "dbus", but that has not been implemented.
