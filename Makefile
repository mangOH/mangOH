# MangOH makefile
#
# Requirements:
#  * Use leaf, or...
#  * Set LEGATO_ROOT environment variable to the path to a Legato working copy.
#  * Set XXXX_SYSROOT environment variables to the appropriate sysroot for the module. For example:
#    "export WP76XX_SYSROOT=/opt/swi/y22-ext-wp76xx/sysroots/armv7a-neon-poky-linux-gnueabi"
#  * If the Legato version in LEGATO_ROOT is < 18.02.0, then it will be necessary to do "export
#    LEGATO_SYSROOT=$XXXX_SYSROOT" where XXXX is the module the build is for.
#

MKSYS_ARGS_COMMON = --object-dir=build/$@ --output-dir=build/update_files/$@

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(CURDIR)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)

# NOTE: This isn't needed when using leaf.
export PATH := $(LEGATO_ROOT)/bin:$(PATH)

BOARDS = green red yellow
MODULES = wp85 wp750x wp76xx wp77xx

GREEN_GOALS = $(foreach module,$(MODULES),green_$(module))
RED_GOALS = $(foreach module,$(MODULES),red_$(module))
YELLOW_GOALS = $(foreach module,$(MODULES),yellow_$(module))

ALL_COMBINATIONS = $(GREEN_GOALS) $(RED_GOALS) $(YELLOW_GOALS)

.PHONY: all $(ALL_COMBINATIONS)
all: $(ALL_COMBINATIONS)

# Until Legato allows external builds in mdefs we will need to build a
# subsystem driver like Cypress Wifi in the top Makefile and with scripts
# define a function to build the Cypress driver for different architectures

# $(call cyp_bld,$(board)_$(module))
# E.g., $(call cyp_bld,yellow_wp76xx)
define cyp_bld
	if [ ! -d build/$1/modules/cypwifi ] ; then \
		mkdir -p build/$1/modules/cypwifi ;\
		cp -pr linux_kernel_modules/cypwifi build/$1/modules/ ; \
		build/$1/modules/cypwifi/build.sh $1 clean; \
	else \
		build/$1/modules/cypwifi/build.sh $1 modules; \
	fi
endef

LEGATO ?= 1
.PHONY: legato_%
legato_%:
	$(eval export LEGATO_TARGET := $(subst legato_,,$@))
ifeq ($(LEGATO),0)
	echo "Not building LEGATO due to \$$LEGATO == 0"
else
	$(MAKE) -C $(LEGATO_ROOT) framework_$(LEGATO_TARGET)
endif

# Build goals that get the target WP module type from the LEGATO_TARGET environment variable.
.PHONY: $(BOARDS)
$(BOARDS):
ifndef LEGATO_TARGET
	$(error LEGATO_TARGET not specified and no goal like $@_wp76xx provided)
endif
	if [ "$@" = "yellow" ] ; then $(call cyp_bld,$@_$(LEGATO_TARGET)) ; fi
	mksys -t $(LEGATO_TARGET) $(MKSYS_ARGS_COMMON) $@.sdef

$(GREEN_GOALS): green_%: legato_%
	# NOTE: When using leaf, these TOOLCHAIN_X variables aren't needed.
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* prefix) \
	mksys -t $* $(MKSYS_ARGS_COMMON) green.sdef

$(RED_GOALS): red_%: legato_%
	# NOTE: When using leaf, these TOOLCHAIN_X variables aren't needed.
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* prefix) \
	mksys -t $* $(MKSYS_ARGS_COMMON) red.sdef

$(YELLOW_GOALS): yellow_%: legato_%
	# Build the Cypress WiFi driver.
	$(call cyp_bld,$@)
	# NOTE: When using leaf, these TOOLCHAIN_X variables aren't needed.
	TOOLCHAIN_DIR=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* dir) \
	TOOLCHAIN_PREFIX=$(shell $(LEGATO_ROOT)/bin/findtoolchain $* prefix) \
	mksys -t $* $(MKSYS_ARGS_COMMON) yellow.sdef

.PHONY: clean
clean:
	rm -rf build
