IIO Kernel Module Definitions
=============================

The source for the IIO kernel modules is part of the standard Legato Linux
kernel source tree, but the module is not built by default.  As a result, we
build this as an out of tree kernel module, but we don't want to version a copy
of the source files.

Instead, it is suggested to put a symlink to each of the sdef files in `[yocto
working copy]/kernel/drivers/iio/` and then refer to the mdef files in the sdef
using an environment variable.  For example:
`${LEGATO_KERNELSRC}/drivers/iio/iio`.

The IIO modules are required by the BMP280 pressure sensor driver module.


