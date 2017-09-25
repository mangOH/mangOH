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
1. `cd` to the legato folder and run `make wp85`
1. Run `source bin/configlegatoenv` to setup the environment variables
1. `cd` to the home directory and clone the mangOH source code by running `git clone --recursive git://github.com/mangOH/mangOH`
1. `cd` into the mangOH folder and run `make red_wp85` or `make green_wp85` depending on which board is being targeted.
1. Run `instsys mangOH_Red.wp85.update 192.168.2.2` to install the system onto the mangOH connected via a USB cable to the CF3 USB port.  Change to `instsys mangOH_Green.wp85.update 192.168.2.2` for mangOH Green.

## SPI
To make use of the SPI bus from userspace, there is a SpiService in Legato. To enable use of this
service, the `spidev` module must be installed by running `modprobe spidev` and then `app start
spiService`.
