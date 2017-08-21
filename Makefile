export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = -s ${MANGOH_ROOT}/apps/GpioExpander/gpioExpanderService
MKSYS_ARGS_GREEN =
MKSYS_ARGS_RED = -s ${MANGOH_ROOT}/apps/RedSensorToCloud


.PHONY: all
all: green_wp85 green_wp750x red_wp85 red_wp750x

.PHONY: green_wp85
green_wp85:
	mksys -t wp85 ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_GREEN} mangOH_Green.sdef

.PHONY: green_wp750x
green_wp750x:
	mksys -t wp750x ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_GREEN} mangOH_Green.sdef

.PHONY: red_wp85
red_wp85:
	mksys -t wp85 ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_RED} mangOH_Red.sdef

.PHONY: red_wp750x
red_wp750x:
	mksys -t wp750x ${MKSYS_ARGS_COMMON} ${MKSYS_ARGS_RED} mangOH_Red.sdef

.PHONY: clean
clean:
	rm -rf \
		_build_mangOH_Green \
		mangOH_Green.wp85.update \
		mangOH_Green.wp750x.update \
		_build_mangOH_Red \
		mangOH_Red.wp85.update \
		mangOH_Red.wp750x.update
