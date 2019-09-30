# mangOH Waveshare E-Ink Linux Framebuffer driver

mangOH Red/Yellow (note separate drivers exist for Red & Yellow) drivers for the Waveshare 2.13 E-Ink Display
Code is experimental so far and the instructions below are tailored to the Yellow and WP7[67]xx. TODO, fix for Red.
Note, original code-base was for WP85 on Red, so should be able to adapt.

### 1. Dependency on meta-mangOH for FB core & helper drivers - standard mangOH spk should have already.

### 2. Need to include sinc/waveshare_eink.sinc in the yellow.sdef and build it.

    Note for Legato 19.04 please add the waveshare_eink.sinc to the end of the file as
    mksys crashes in the build if it is added in the beginning of the yellow.sdef.

### 3. Tested on Legato 19.04 with these shell startup/stop scripts

    scp(1) scripts/start_eink.sh/stop_eink.sh to the target.
    Need to run the start_eink.sh after bootup from a shell to load the kernel loadable
    modules, mangOH_yellow_ws213, fb_waveshare_eink, and finally EinkDhubIf app in that order.
    To remove just run stop_eink.sh and then app stop EinkDhubIf in that order.

### 4. Currently is ready for auto-startup on Legato 19.07 as it allows mdef start/stop scripts.

    Note, kludges are noted in the mangOH_yellow_ws213 start script in regards
    to the SPI busy port and other issues. mangOH_yellow_ws213 needs to deal with the
    SPI bus renumbering off of the sx1509q.

### 6. Please see TODO.md for more things to fix.
