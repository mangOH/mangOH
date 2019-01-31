# Waveshare E-Ink Driver TODO

## Questions


## Fixes Requested
Fix `xres`, `yres`, `xres_virtual`, `yres_virtual` to be correct based upon a
128x250 addressable range and a 122x250 visible range. This will require adding
more properties to `struct waveshare_eink_device_properties` Care must be taken
to update any calculations in the driver to make sure that they are using the
correct values.

The variables `width`, `height` and `bpp` should not be defined at the scope of
the `fb_waveshare_eink.c` file. Instead, they should be part of the data
associated with the device. The reason for this is that if you had two displays
connected simultaneously that had different width, height or bpp specifications,
the driver would not work correctly because the variables would be initialized
with the values associated with whichever device was created last.

Are the LUT tables valid for all of the devices listed as supported in the
`devices` array? If we aren't confident that devices other than the ws213 will
work with this driver, we should not claim to support them.


## Enhancements Requested
Add "deep sleep" suppor to the driver. It seems that this is important based
upon this FAQ entry on the
[wiki page](https://www.waveshare.com/wiki/2.13inch_e-Paper_HAT).

> Question:
> Why my e-paper has ghosting problem after working for some days
> Answer:
> Please set the e-paper to sleep mode or disconnect it if you needn't refresh
> the e-paper but need to power on your development board or Raspberry Pi for
> long time. Otherwise, the voltage of panel keeps high and it will damage the
> panel.

It's not clear to me how deep sleep would fit into the framebuffer model. Maybe
the driver should start a timer after each write and if no subsequent write
arrives befor the timer expires, then the device is put into deep sleep. I
wonder how perceptible the delay to resume from deep sleep would be. If it is
quite slow, then we need to be careful that we don't go to sleep too quickly.


It would be nice if the probe function did the following (in addition to what it
already does):
1. Send the Status Bit Read (0x2F) command and read the byte that is returned.
   Bitfield [1:0] is the chip ID field and should have the value of 1. Note that
   this is a write of one byte followed by a read of one byte. This is the only
   place in the driver where we read anything.
1. Send the Panel Break Detection (0x23) command. The datasheet indicates that
   CLKEN=1 is required, so perhaps it's necessary to send the Display Update
   Control 2 command (0x22) to enable the clock first. Unfortunately, the
   individual bits of the Display Update Control 2 command are not clearly
   documented, so it may be difficult to do this.
1. Wait for the BUSY GPIO to go low
1. Send the Status Bit Read (0x2F) command again and read the byte that is
   returned. If bit 3 is set, then the panel is broken.

Doing this would give us greater confidence that we're talking to the device
that we think we are and that the device is functional.


## Challenges
How should we deal with the fact that this driver depends on features of the kernel that aren't
explicitly selectable using menuconfig? I doubt the system reference team would be happy about us
enabling config for a bunch of device drivers for devices that we don't intend to use so that the
side-effect of getting `sys_imageblit()` (for example) included into the kernel is achieved.
