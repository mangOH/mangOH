# mangOH Makefile
#
# Requirements:
#  * Use leaf, or...
#  * Set LEGATO_ROOT environment variable to the path to a Legato working copy.
#  * Set XXXX_SYSROOT environment variables to the appropriate sysroot for the module. For example:
#    "export WP76XX_SYSROOT=/opt/swi/y22-ext-wp76xx/sysroots/armv7a-neon-poky-linux-gnueabi"
#  * If the Legato version in LEGATO_ROOT is < 18.02.0, then it will be necessary to do "export
#    LEGATO_SYSROOT=$XXXX_SYSROOT" where XXXX is the module the build is for.
#
# By default, the Legato framework will be built before building the mangOH software.
# Set LEGATO=0 to skip this step.

# Arguments passed to mksys whenever it is invoked.
MKSYS_ARGS_COMMON = --object-dir=build/$@ --output-dir=build/update_files/$@

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(CURDIR)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)

# NOTE: This isn't needed when using leaf.
export PATH := $(LEGATO_ROOT)/bin:$(PATH)

# List of all supported mangOH boards.
BOARDS = green red yellow

# List of all supported Sierra Wireless WP-series modules.
MODULES = wp85 wp750x wp76xx wp77xx

# List of goals for each board, in the form $(board)_$(module), like green_wp85.
GREEN_GOALS = $(foreach module,$(MODULES),green_$(module))
RED_GOALS = $(foreach module,$(MODULES),red_$(module))
YELLOW_GOALS = $(foreach module,$(MODULES),yellow_$(module))

# By default (if no goal is specified on the command line), we try to build for all combinations
# of boards and targets.  This will probably not work for most people, though, because they won't
# have all the necessary toolchains installed.
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

# Rule to build the Legato framework, if $(LEGATO) is not 0.
# WARNING: Do not use LEGATO=1 when using a Legato provided by leaf. This will result in the
# modification of files within ~/leaf-data which may be shared between many leaf workspaces and leaf
# profiles.
LEGATO ?= 0
.PHONY: legato_%
legato_%:
	$(eval export LEGATO_TARGET := $(subst legato_,,$@))
ifeq ($(LEGATO),0)
	echo "Not building LEGATO due to \$$LEGATO == 0"
else
	$(MAKE) -C $(LEGATO_ROOT) framework_$(LEGATO_TARGET)
endif

# Build goals that get the target WP module type from the LEGATO_TARGET environment variable.
# If LEGATO_TARGET is defined (e.g., when using leaf), then you can run 'make yellow', for example.
.PHONY: $(BOARDS)
$(BOARDS): %: %_$(LEGATO_TARGET)

# If LEGATO_TARGET is not set, and someone tries to make one of the board goals (like 'make yellow')
# then it will attempt to build something like 'yellow_'.  In that case, we want to report a
# helpful error message, and we use these "missing LEGATO_TARGET" goals to do that.
MISSING_LEGATO_TARGET = $(foreach board,$(BOARDS),$(board)_)
.PHONY: $(MISSING_LEGATO_TARGET)
$(MISSING_LEGATO_TARGET):
	$(error LEGATO_TARGET not specified and no goal like $@wp76xx provided)

# Build goals that don't rely on LEGATO_TARGET, instead allowing the module to be specified
# on the command line as part of the goal. E.g., 'make green_wp85'.  If using these, Legato's
# findtoolchain script will be used to try to find the approriate build toolchain.

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

# The cleaning goal.
.PHONY: clean
clean:
	rm -rf build

# Goals for building the software to be used when testing inside the environmental test chamber.
TEST_BUILD_ID = $@_$(LEGATO_TARGET)
.PHONY: env_test_yellow
env_test_yellow: env_test_%:
ifndef LEGATO_TARGET
	$(error LEGATO_TARGET not specified)
endif
	$(call cyp_bld,$*_$(LEGATO_TARGET))
	mksys -t $(LEGATO_TARGET) \
		  --object-dir=build/$(TEST_BUILD_ID) \
		  --output-dir=build/update_files/$(TEST_BUILD_ID) \
	      $@.sdef
