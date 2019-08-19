# bluezDBus Component

This Legato component contains [D-Bus introspection XML](
https://dbus.freedesktop.org/doc/dbus-specification.html#introspection-format) for the D-Bus
interfaces relating to BlueZ. Interfaces have been added as needed, so some interfaces may missing.
The XML files were created in part by hand authoring based on the Bluez documentation
(`doc/*-api.txt`) and in part by calling the `Introspectable.Introspect` method on interfaces
published by bluetoothd. This can be achieved by running a command like this:
```
dbus-send --system --type=method_call --print-reply --dest=org.bluez /org/bluez/hci0 org.freedesktop.DBus.Introspectable.Introspect
```

This component uses the `gdbus-codegen` tool to generate C language bindings to the interfaces
described in the introspection XML files. The bindings are for both the client and server side of
the interface.
