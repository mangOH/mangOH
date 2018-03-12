# MangOH makefile
#
# Requirements:
#  * Set LEGATO_ROOT environment variable to the path to a Legato working copy.
#  * Set XXXX_SYSROOT environment variables to the appropriate sysroot for the module. For example:
#    "export WP76XX_SYSROOT=/opt/swi/y22-ext-wp76xx/sysroots/armv7a-neon-poky-linux-gnueabi"
#  * If the Legato version in LEGATO_ROOT is < 18.02.0, then it will be necessary to do "export
#    LEGATO_SYSROOT=$XXXX_SYSROOT" where XXXX is the module the build is for.

export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = --object-dir=build/${@} -s $(MANGOH_ROOT)/apps/GpioExpander/gpioExpanderService
MKSYS_ARGS_GREEN = --output-dir=build/update_files/green
MKSYS_ARGS_RED = --output-dir=build/update_files/red -s $(MANGOH_ROOT)/apps/RedSensorToCloud

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(MANGOH_ROOT)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_GREEN)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_RED)

export PATH := $(LEGATO_ROOT)/bin:$(PATH)

# green_wp76xx and green_wp77xx are excluded because gpioExpanderService won't compile due to the
# removal of i2c-utils in wp76 and wp77 yocto.
.PHONY: all
all: green_wp85 green_wp750x red_wp85 red_wp750x red_wp76xx red_wp77xx

LEGATO ?= 1
.PHONY: legato_%
legato_%:
ifeq ($(LEGATO),0)
	echo "Not building LEGATO due to \$$LEGATO == 0"
else
	$(eval LEGATO_TARGET := $(subst legato_,,$@))
	$(MAKE) -C $(LEGATO_ROOT) framework_$(LEGATO_TARGET)
endif

.PHONY: module_env_%
module_env_%: legato_%
	$(eval MODULE := $(subst module_env_,,$@))
	$(eval MODULE_UPPER := $(shell echo $(MODULE) | tr [a-z] [A-Z]))
	$(eval $(MODULE_UPPER)_TOOLCHAIN_DIR := $(shell $(LEGATO_ROOT)/bin/findtoolchain $(MODULE) dir))
	$(eval $(MODULE_UPPER)_TOOLCHAIN_PREFIX := $(shell $(LEGATO_ROOT)/bin/findtoolchain $(MODULE) prefix))
	$(eval TOOLCHAIN_DIR := $($(MODULE_UPPER)_TOOLCHAIN_DIR))
	$(eval TOOLCHAIN_PREFIX := $($(MODULE_UPPER)_TOOLCHAIN_PREFIX))
	echo "TOOLCHAIN_DIR=$(TOOLCHAIN_DIR), TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX)"
ifeq ($(LEGATO_SYSROOT),)
	$(eval LEGATO_SYSROOT := $($(MODULE_UPPER)_SYSROOT))
endif

.PHONY: green_wp85
green_wp85: module_env_wp85
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=GREEN \
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp750x
green_wp750x: legato_wp750x module_env_wp750x
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=GREEN \
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp76xx
green_wp76xx: legato_wp76xx module_env_wp76xx
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=GREEN \
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp77xx
green_wp77xx: legato_wp77xx module_env_wp77xx
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=GREEN \
	mksys -t wp77xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: red_wp85
red_wp85: legato_wp85 module_env_wp85
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=RED \
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp750x
red_wp750x: legato_wp750x module_env_wp750x
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=RED \
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp76xx
red_wp76xx: legato_wp76xx module_env_wp76xx
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=RED \
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp77xx
red_wp77xx: legato_wp77xx module_env_wp77xx
	TOOLCHAIN_DIR=$(TOOLCHAIN_DIR) \
	TOOLCHAIN_PREFIX=$(TOOLCHAIN_PREFIX) \
	MANGOH_BOARD=RED \
	mksys -t wp77xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: clean
clean:
	rm -rf build
