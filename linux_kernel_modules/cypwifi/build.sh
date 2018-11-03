#
# We Assume that the user has run cfglegato previously to make many
# environment variables exportable

# Cypress/Broadcom have there own set of environment vars that they need
# some are even defined the same - would be good to fix
# If your kernel headers are in a different place than mine - please
# modify the values appropriately
set -x
export PATH=`findtoolchain ${LEGATO_TARGET} dir`:$PATH
export MY_KERNEL=`findtoolchain wp85 kernelroot`
export KLIB=$MY_KERNEL
export KLIB_BUILD=$MY_KERNEL

# Need to change these if you want a different X-chain
export ARCH=arm
export CROSS_COMPILE=`findtoolchain ${LEGATO_TARGET} prefix`


# Lets do it
OLDPWD=`pwd`
cd ${MANGOH_ROOT}/linux_kernel_modules/cypwifi
make clean ; make defconfig-brcmfmac ; make modules
if [ $? -ne 0 ] ; then
	exit 1
else
	cd $OLDPWD
fi

