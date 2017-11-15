#
#*******************************************************************************
# Name:  Makefile.master
#
# Notes: Makefile for mt7697 kernel module. Include files for this module must
# point to WL18xx includes from TIs kernel driver tree.
#===============================================================================
#
# Dragan Marinkovic (dragan@marisol-sys.com)
#
#*******************************************************************************
#

# Driver version
DRV_VERSION=1.0.0

#
# Note that we could have used KCFLAGS instead of CC_OPTS, and put KCFLAGS on
# make line.
#

# Make sure that arm-poky-linux-gnueabi-gcc is on you path. THe default location is:
# /opt/swi/y17-ext/sysroots/x86_64-pokysdk-linux/usr/bin/arm-poky-linux-gnueabi

# The location of the kernel build directory, should be some kind of environment
# variable
KDIR := /home/david/kernel

# Root of TI driver module driver sources
TI_DRIVER_ROOT := /home/david/yocto-1.7/build_bin/tmp/work/swi_mdm9x15-poky-linux-gnueabi/ti-compat-wireless/1.0-r0/ti-compat-wireless/src/compat_wireless

# Root of MT7697 SPI queue module driver sources
MT7697_QUEUES_ROOT := /home/david/mangOH/legato/drivers/mangoh/mt7697q

# Pass version information to module
EXTRA_CFLAGS += -DVERSION=\"$(DRV_VERSION)\" -DDEBUG
EXTRA_CFLAGS += -DCPTCFG_NL80211_TESTMODE -DCPTCFG_CFG80211_DEFAULT_PS -DCPTCFG_CFG80211_DEBUGFS -DCPTCFG_CFG80211_WEXT
EXTRA_CFLAGS += -DCPTCFG_WEXT_CORE -DCPTCFG_WEXT_PROC -DCPTCFG_WEXT_SPY -DCPTCFG_WEXT_PRIV

# Kernel tree comes with some "defined but not used funnctions" warnings and
# we are going to supress it here.
EXTRA_CFLAGS += -Wno-unused-function

# It is good idea to make sure that there are no warnings
# EXTRA_CFLAGS += -Werror

# Add some missing include paths
# EXTRA_CFLAGS += -Idrivers/net/wireless/ti -I$(MODROOT)/wl12xx -I$(MODROOT) -I$(MODROOT)/drivers/net/wireless/ti
EXTRA_CFLAGS += -I$(TI_DRIVER_ROOT) -I$(MT7697_QUEUES_ROOT)
EXTRA_CFLAGS += -I$(TI_DRIVER_ROOT)/include -I$(TI_DRIVER_ROOT)/include/uapi

# Make kernel build verbose
VERBOSE += 1

TARGET        := mt7697wifi_core
PWD           := $(shell pwd)
ARCH          := arm
CROSS_COMPILE := arm-poky-linux-gnueabi-
CC            := $(CROSS_COMPILE)gcc
CC_OPTS       := -O3 -Wall $(EXTRA_CFLAGS)
STRIP         := $(CROSS_COMPILE)strip
MAKE          := make


# Add all object files we need to make *.ko module file. Multiple lines using
# obj-m += ... would work as well.
$(TARGET)-objs := wmi.o cfg80211.o main.o txrx.o
obj-m := $(TARGET).o

all: modules

modules:
	$(MAKE) -C $(KDIR) KBUILD_VERBOSE=$(VERBOSE) SUBDIRS=$(PWD) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)

dist-clean: clean
	@rm -f *~

clean:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) M=$(PWD) ARCH=$(ARCH) \
    CROSS_COMPILE=$(CROSS_COMPILE) clean
	@rm -f $(TARGET).o
	@rm -f $(TARGET).mod.c
	@rm -f $(TARGET).mod.o
	@rm -f $(TARGET).ko
	@rm -f .*.cmd
	@rm -f Module.symvers
	@rm -rf .tmp_versions

.PHONY: modules clean dist-clean
