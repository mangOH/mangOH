# Waveshare E-Ink Driver TODO

## Questions
What is the purpose of the `#ifdef __LITTLE_ENDIAN` sections in the driver? Add code comments to
clarify.


## Fixes Requested
* I think `eink_device->max_speed_hz` is still being initialized incorrectly. It should be
  initialized with the max speed supported by the device, not the max speed supported by the SPI
  master.


## Enhancements Requested


## Challenges
How should we deal with the fact that this driver depends on features of the kernel that aren't
explicitly selectable using menuconfig? I doubt the system reference team would be happy about us
enabling config for a bunch of device drivers for devices that we don't intend to use so that the
side-effect of getting `sys_imageblit()` (for example) included into the kernel is achieved.
