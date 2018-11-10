# MangOH makefile
#
# Requirements:
#  * Set LEGATO_ROOT environment variable to the path to a Legato working copy.
#  * Set XXXX_SYSROOT environment variables to the appropriate sysroot for the module. For example:
#    "export WP76XX_SYSROOT=/opt/swi/y22-ext-wp76xx/sysroots/armv7a-neon-poky-linux-gnueabi"
#  * If the Legato version in LEGATO_ROOT is < 18.02.0, then it will be necessary to do "export
#    LEGATO_SYSROOT=$XXXX_SYSROOT" where XXXX is the module the build is for.

export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = --object-dir=build/${@}
MKSYS_ARGS_GREEN = --output-dir=build/update_files/green
MKSYS_ARGS_RED = --output-dir=build/update_files/red
MKSYS_ARGS_YELLOW = --output-dir=build/update_files/yellow

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(MANGOH_ROOT)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_GREEN)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_RED)

export PATH := $(LEGATO_ROOT)/bin:$(PATH)

.PHONY: all
all:  green_wp85  green_wp750x  green_wp76xx  green_wp77xx \
        red_wp85    red_wp750x    red_wp76xx    red_wp77xx \
     yellow_wp85 yellow_wp750x yellow_wp76xx yellow_wp77xx

LEGATO ?= 1
.PHONY: legato_%
legato_%:
ifeq ($(LEGATO),0)
	echo "Not building LEGATO due to \$$LEGATO == 0"
else
	$(eval LEGATO_TARGET := $(subst legato_,,$@))
	$(MAKE) -C $(LEGATO_ROOT) framework_$(LEGATO_TARGET)
endif

export LEGATO_TARGET := $(subst legato_,,$@)

.PHONY: green_wp85
green_wp85: legato_wp85
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 prefix) \
	MANGOH_BOARD=green \
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp750x
green_wp750x: legato_wp750x
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp750x dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp750x prefix) \
	MANGOH_BOARD=green \
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp76xx
green_wp76xx: legato_wp76xx
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx prefix) \
	MANGOH_BOARD=green \
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp77xx
green_wp77xx: legato_wp77xx
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx prefix) \
	MANGOH_BOARD=green \
	mksys -t wp77xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: red_wp85
red_wp85: legato_wp85
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 prefix) \
	MANGOH_BOARD=red \
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp750x
red_wp750x: legato_wp750x
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp750x dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp750x prefix) \
	MANGOH_BOARD=red \
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp76xx
red_wp76xx: legato_wp76xx
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx prefix) \
	MANGOH_BOARD=red \
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp77xx
red_wp77xx: legato_wp77xx
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx prefix) \
	MANGOH_BOARD=red \
	mksys -t wp77xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

# Until Legato allows external builds in mdefs we will need to build a
# subsystem driver like Cypress Wifi in the top makefile and with scripts
# Define a function to build the Cypress driver for different architectures

# $(call cyp_bld,build_dir)
define cyp_bld
	if [ ! -d $(MANGOH_ROOT)/build/$1/modules/cypwifi ] ; then \
		mkdir -p $(MANGOH_ROOT)/build/$1/modules/cypwifi ;\
		cp -pr $(MANGOH_ROOT)/linux_kernel_modules/cypwifi $(MANGOH_ROOT)/build/$1/modules/ ; \
		$(MANGOH_ROOT)/build/$1/modules/cypwifi/build.sh $1 clean; \
	else \
		$(MANGOH_ROOT)/build/$1/modules/cypwifi/build.sh $1 modules; \
	fi
endef

.PHONY: yellow_wp85
yellow_wp85: legato_wp85
	$(call cyp_bld,$@)
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp85 prefix) \
	MANGOH_BOARD=yellow \
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: yellow_wp750x
yellow_wp750x: legato_wp750x
	$(call cyp_bld,$@)
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp750x prefix) \
	MANGOH_BOARD=yellow \
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: yellow_wp76xx
yellow_wp76xx: legato_wp76xx
	$(call cyp_bld,$@)
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp76xx prefix) \
	MANGOH_BOARD=yellow \
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_YELLOW) mangOH.sdef

.PHONY: yellow_wp77xx
yellow_wp77xx: legato_wp77xx
	$(call cyp_bld,$@)
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain wp77xx prefix) \
	MANGOH_BOARD=yellow \
	mksys -t wp77xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_YELLOW) mangOH.sdef

.PHONY: clean
clean:
	rm -rf build
