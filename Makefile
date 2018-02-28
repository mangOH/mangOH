export MANGOH_ROOT = $(shell pwd)

MKSYS_ARGS_COMMON = --object-dir=build/${@} -s $(MANGOH_ROOT)/apps/GpioExpander/gpioExpanderService
MKSYS_ARGS_GREEN = --output-dir=build/update_files/green
MKSYS_ARGS_RED = --output-dir=build/update_files/red -s $(MANGOH_ROOT)/apps/RedSensorToCloud

# The comments below are for Developer Studio integration. Do not remove them.
# DS_CLONE_ROOT(MANGOH_ROOT)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_COMMON)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_GREEN)
# DS_CUSTOM_OPTIONS(MKSYS_ARGS_RED)


.PHONY: green_wp85
green_wp85:
	MANGOH_BOARD=GREEN mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp750x
green_wp750x:
	MANGOH_BOARD=GREEN mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: green_wp76xx
green_wp76xx:
	MANGOH_BOARD=GREEN mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_GREEN) mangOH.sdef

.PHONY: red_wp85
red_wp85:
	MANGOH_BOARD=RED mksys -t wp85 $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp750x
red_wp750x:
	MANGOH_BOARD=RED mksys -t wp750x $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: red_wp76xx
red_wp76xx:
	MANGOH_BOARD=RED mksys -t wp76xx $(MKSYS_ARGS_COMMON) $(MKSYS_ARGS_RED) mangOH.sdef

.PHONY: clean
clean:
	rm -rf build
