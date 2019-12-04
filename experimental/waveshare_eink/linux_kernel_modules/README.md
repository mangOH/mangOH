# mangOH Waveshare E-Ink Linux Framebuffer driver

mangOH Red/Yellow (note separate drivers exist for Red & Yellow) drivers for the Waveshare 2.13 E-Ink Display
Code is experimental so far and the instructions below are tailored to the Yellow and WP77xx. TODO, fix for Red
and test on WP76xx.  Note, original code-base was for WP85 on Red, so should be able to adapt.

### 1. Dependency on meta-mangOH for FB core & helper drivers - standard mangOH spk should have already.

### 2. Need to include sinc/waveshare_eink.sinc in the yellow.sdef and build it.

    Note for Legato 19.04 please add the waveshare_eink.sinc to the end of the file as
    mksys crashes in the build if it is added in the beginning of the yellow.sdef.

### 3. Currently is ready for auto-startup on Legato 19.07 as it allows mdef start/stop scripts.

    Note, kludges are noted in the mangOH_yellow_ws213 start script in regards
    to the SPI busy port and other issues. mangOH_yellow_ws213 needs to deal with the
    SPI bus renumbering off of the sx1509q.

### 4. Please see TODO.md for more things to fix.
