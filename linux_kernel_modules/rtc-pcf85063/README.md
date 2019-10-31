RTC-PCF85063 Linux Kernel Driver
==========================

This version of the RTC-PCF85063 Linux kernel driver was extracted from the mainline kernel at tag v4.9.125. The content were copied from `drivers/rtc/rtc-pcf85063.c`.

We have added functionality to allow control of the square-wave clkout frequencies via sysfs entries.
Note, this was developed in the context of the mangOH Yellow board which has the square-wave clkout pin
connected to a buzzer. Thus, on yellow one can drive the buzzer on the allowable frequencies. The datasheet
enumerates other applications needing the square-wave output functionality.

Note, we have not followed the common rtc chip driver square-wave output functionality which
has been required to recreate the common clock framework on each instance. To us it does
seem like overkill to use for a simple square-wave output functionality - thus, one would
need to remove this code for entry to the mainline kernel.
