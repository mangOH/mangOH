# Waveshare E-Ink Driver TODO

## Questions


## Fixes Requested


## Enhancements Requested


## Challenges
How should we deal with the fact that this driver depends on features of the kernel that aren't
explicitly selectable using menuconfig? I doubt the system reference team would be happy about us
enabling config for a bunch of device drivers for devices that we don't intend to use so that the
side-effect of getting `sys_imageblit()` (for example) included into the kernel is achieved.
