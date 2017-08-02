all:
	$(info Bug LE-7663 prevents this Makefile from building a correct Legato system.)
	$(info Once this bug is fixed in a stable version of Legato, the Makefile will)
	$(info be restored.  For now, build the mangOH system by executing the following)
	$(info command from a Legato working copy:)
	$(info     make wp85 SDEF_TO_USE=$$MANGOH_ROOT/mangOH_Red.sdef MKSYS_FLAGS="-s $$MANGOH_ROOT/apps/GpioExpander/gpioExpanderService -s $$MANGOH_ROOT/apps/RedSensorToCloud")
	$(info Note that you must first define MANGOH_ROOT.  eg. export MANGOH_ROOT=~/mangOH)
	exit 1
