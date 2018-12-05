## Linux kernel driver Cypress Linux WiFi Driver Release (FMAC)

* Datasheet: http://www.cypress.com/documentation/datasheets/cyw43364-single-chip-ieee-80211-bgn-macbasebandradio

* Cypress tar-ball based on: https://community.cypress.com/docs/DOC-15932

* The version of the chip used on the MangOH Yellow is from USI which incorporates
	both Bluetooth & Wifi in a SIP package.

* Porting details
  * We have kept this as a sub-system driver and not integrated with the Legato Kernel
	Module Build Architecture such that newer quarterly releases from the
	Cypress Developers can be upgraded to. Thus, it is using preBuilt modules.
  * Presently, Legato does not allow external Builds from the mdef file, thus, we have
	modified the top-level Makefile to build the driver before building the target.
  * We have had to make a few changes to the backport code to deal with incorrect
	semantic patching for the different kernels on the WP85 & WP76.
  * We have selected wlan1 for the Cypress chip. We cannot use the lowest numbered interface
	as the system reference code has assumed the TI Wifi on the IOT card is wlan0
	Thus, we picked the next interface.

* Currently, the Cypress chip cannot operate in low-power mode on the WP85. It seems
    to be some sort of SDIO timing issue that Cypress will be looking at on the
    next Rev. of the Yellow board. For now we have commented-out all calls to turn
    on power control.

* The Broadcom/Cypress uses the WL_REG_ON gpio to brought out of reset, but
    needs 2 sleep cycles after VBAT & VDDIO is applied before it is
    turned on. Both the WP76 & 85 are setup to connect GPIO 8 to WL_REG_ON

* The SDIO stacks are set up differently on the WP76 & WP85 beecause different SDIO
    controllers were implemented on the SOC's. The MSM-SDCC on the WP85 starts at boot
    and never re-scans unless kick-started, so we unload & reload the module. Whereas
    the WP76 SDIO controller runs in a continuous 1-second poll loop. It would be good
    to remove the polling for low-power reasons.

* We have followed a customized way for the firmware search path. Later releases will
    have the standard firmware search path used. If you have other firmware that needs
    to load afterward please beware.

* We have not integrated fully with the Legato Wifi architecture. TODO: need to work on
    the glue code with Legato Patches.

* Testing was done simply with the following commands:
  * /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh start
  * wpa_supplicant -B -Dnl80211 -iwlan1 -c /etc/wpa_supplicant.conf
  * /sbin/udhcpc -R -b -i wlan1
  * ping www.google.com

