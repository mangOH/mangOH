#
# We Assume that the user has run cfglegato previously to make many
# environment variables exportable

# Cypress/Broadcom have there own set of environment vars that they need
# some are even defined the same - would be good to fix
# If your kernel headers are in a different place than mine - please
# modify the values appropriately

USAGE="Usage: $0 build_target_dir clean|modules"

if [ -z "$1" ] ; then
	echo $USAGE
	exit 2
fi

if [ -z "$2" ] ; then
	echo $USAGE
	exit 3
fi

FINDTOOLCHAIN=${LEGATO_ROOT}/bin/findtoolchain

export PATH=`${FINDTOOLCHAIN} ${LEGATO_TARGET} dir`:$PATH
export MY_KERNEL=`${FINDTOOLCHAIN} ${LEGATO_TARGET} kernelroot`
export KLIB=$MY_KERNEL
export KLIB_BUILD=$MY_KERNEL

# Need to change these if you want a different X-chain
export ARCH=arm
export CROSS_COMPILE=`${FINDTOOLCHAIN} ${LEGATO_TARGET} prefix`


# Lets do it
OLDPWD=`pwd`
cd ${MANGOH_ROOT}/build/${1}/modules/cypwifi
if [ "$2" == "clean" ] ; then
	make defconfig-brcmfmac
fi
make modules

if [ $? -ne 0 ] ; then
	cd $OLDPWD
	exit 1
else
	cd $OLDPWD
fi

