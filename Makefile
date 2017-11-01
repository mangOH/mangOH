export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = -s $(MANGOH_ROOT)/apps/GpioExpander/gpioExpanderService
MKSYS_ARGS_GREEN =
MKSYS_ARGS_RED = -s $(MANGOH_ROOT)/apps/RedSensorToCloud

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(MANGOH_ROOT)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_GREEN)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_RED)

# This is a temporary workaround for bug LE-7850. Once Legato 17.08.0 or 17.07.2 is released, this
# should no longer be necessary.
export TARGET := wp85

.PHONY: all
all: green_wp85 green_wp750x green_wp76xx red_wp85 red_wp750x red_wp76xx

.PHONY: green_wp85
green_wp85:
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH_Green.sdef

.PHONY: green_wp750x
green_wp750x:
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH_Green.sdef

.PHONY: green_wp76xx
green_wp76xx:
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH_Green.sdef

.PHONY: red_wp85
red_wp85:
	mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH_Red.sdef

.PHONY: red_wp750x
red_wp750x:
	mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH_Red.sdef

.PHONY: red_wp76xx
red_wp76xx:
	mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH_Red_9x07.sdef

.PHONY: clean
clean:
	rm -rf \
		_build_mangOH_Green \
		mangOH_Green.wp85.update \
		mangOH_Green.wp750x.update \
		mangOH_Green.wp76xx.update \
		_build_mangOH_Red \
		_build_mangOH_Red_9x07 \
		mangOH_Red.wp85.update \
		mangOH_Red.wp750x.update \
		mangOH_Red_9x07.wp76xx.update
