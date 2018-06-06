# MangOH

Base project containing apps and drivers for the mangOH hardware

## Setup

1. Download and install the appropriate toolchain for your WP module from
   [source.sierrawireless.com](https://source.sierrawireless.com)
    1. Click *AirPrime > WP Series > Your WP module > Firmware*
    1. Scroll to near the bottom of the page and follow the *Release X Components* link
    1. Download the 64-bit ToolChain
    1. Run `chmod a+x toolchain.sh` on the toolchain file (name will vary depending on module)
    1. Run the toolchain installer: `./toolchain.sh` and install to
       `/opt/swi/y22-ext_SWI9X07Y_02.14.04.00`. Alter the previous path so that it is consistent
       with the yocto version and Legato Linux distribution version.
    1. Run `ln -sf /opt/swi/y22-ext_SWI9X07Y_02.14.04.00 /opt/swi/y22-ext-wp76xx`, again modifying
       this command slightly to match the toolchain/module.
1. Yocto 1.7 based systems have a problem with their toolchain where some scripts required for
   building kernel modules aren't built. This currently affects the WP85 and WP75 release 15. To
   correct this issue, do the following.
    1. `export PATH=$PATH:/opt/swi/y17-ext-wpXXXX/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi`
       (Note that the *y17-ext-wpXXXX* will be something like *y17-ext-wp750x* depending on the
       module.)
    1. `cd /opt/swi/y17-ext-wpXXXX/sysroots/armv7a-vfp-neon-poky-linux-gnueabi/usr/src/kernel`
    1. `sudo chown -R $USER .`
    1. `ARCH=arm CROSS_COMPILE=arm-poky-linux-gnueabi- make scripts`
    1. `sudo chown -R root .`
1. Get the Legato source code as described by the [Legato
   README](https://github.com/legatoproject/legato-af/blob/master/README.md)
1. Define an environment variable describing the location of the legato framework:
   `export LEGATO_ROOT=~/legato_workspace/legato`
1. `cd` to the home directory and clone the mangOH source code by running
   `git clone --recursive git://github.com/mangOH/mangOH`
1. Build mangOH for your board/module combination by doing using a command like `make red_wp76xx` in
   the mangOH folder. Possible boards are *red* and *green* and possible modules are *wp85*,
   *wp750x*, *wp76xx* and *wp77xx*. Note that this will build the necessary parts of the Legato
   framework from `$LEGATO_ROOT` as well.
1. Run `$LEGATO_ROOT/bin/update build/update_files/red/mangoh.wp76xx.update 192.168.2.2` to program
   the update file to the mangOH board connected via a USB cable to the CF3 USB port. Note that the
   previous command will need to be changed slightly depending on which board and module is in use.
   Also note that it may be convenient to put `$LEGATO_ROOT/bin` into your `$PATH` variable for
   easier access to the `update` command.
