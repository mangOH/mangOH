# BluetoothSensorTag

This is a Legato app which was written to serve as an example of how to interact
with a Bluetooth device from a mangOH Yellow. The mangOH Yellow has an onboard
Bluetooth adapter. [BlueZ](http://www.bluez.org/) (the standard Linux Bluetooth
stack) provides bluetoothd which implements a number of
[D-Bus](https://www.freedesktop.org/wiki/Software/dbus/) interfaces to work with
Bluetooth. This application makes use of GLib's [GIO
D-Bus](https://developer.gnome.org/gio/stable/gdbus-convenience.html)
implementation to call D-Bus methods. Please also see the README.md in the
bluezDBus component.

## SMACK workaround
The `meta-mangoh` layer includes a change to write the following to
`/sys/fs/smackfs/load2` to grant the necessary permissions for the
`bluetoothSensorTag` app to communicate with `bluetoothd` via `dbus-daemon`.
```
_ app.bluetoothSensorTag -w----
app.bluetoothSensorTag _ -w----
```
Writing the above lines allows the app running as smack label
"app.bluetoothSensorTag" to communicate with `dbus-daemon` running as label
"_". This is not a great security practice since many processes run with smack
label "_". Ideally, dbus would run with smack label "dbus", but that has not
been implemented.
