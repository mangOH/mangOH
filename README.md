# MangOH

Base project containing apps and drivers for the mangOH hardware

## Setup

1. Download and install the appropriate toolchain from [source.sierrawireless.com](https://source.sierrawireless.com/resources/legato/downloads/)
1. Enable the toolchain to build kernel modules by running these commands:
    1. `export PATH=$PATH:/opt/swi/y17-ext/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi`
    1. `cd /opt/swi/y17-ext/sysroots/armv7a-vfp-neon-poky-linux-gnueabi/usr/src/kernel`
    1. `sudo chown -R $USER .`
    1. `ARCH=arm CROSS_COMPILE=arm-poky-linux-gnueabi- make scripts`
    1. `sudo chown -R root .`
1. Get the Legato source code as described by the [Legato README](https://github.com/legatoproject/legato-af/blob/master/README.md)
1. Clone the mangOH source code by running `git clone --recursive git://github.com/mangOH/mangOH`
1. `cd` into the Legato working folder and run the following command to build a system for the mangOH Red: `make wp85 SDEF_TO_USE=$MANGOH_ROOT/mangOH_Red.sdef MKSYS_FLAGS="-s $MANGOH_ROOT/apps/GpioExpander/gpioExpanderService -s $MANGOH_ROOT/apps/RedSensorToCloud"`
1. Run `source ./bin/configlegatoenv` to put the Legato tools into `$PATH`
1. Run `instlegato wp85 192.168.2.2` to install the system onto the mangOH connected via a USB cable to the CF3 USB port.
