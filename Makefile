# Set a default target of wp85. To build for another target, set the TARGET
# environment variable to the appropriate target (e.g. export TARGET=wp76xx) or
# pass the variable to make (e.g. make red TARGET=wp750x).
TARGET := wp85

export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = -s ${MANGOH_ROOT}/apps/GpioExpander/gpioExpanderService
MKSYS_ARGS_GREEN =
MKSYS_ARGS_RED = -s ${MANGOH_ROOT}/apps/RedSensorToCloud

.PHONY: all
all: green red

.PHONY: green
green:
	mksys -t ${TARGET} ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_GREEN} mangOH_Green.sdef

.PHONY: red
red:
	mksys -t ${TARGET} ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_RED} mangOH_Red.sdef

.PHONY: clean
clean:
	rm -rf \
		mangOH_Green.*.update \
		_build_mangOH_Green \
		mangOH_Red.*.update \
		_build_mangOH_Red
