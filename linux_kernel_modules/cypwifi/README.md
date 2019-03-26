## Linux kernel driver Cypress Linux WiFi Driver Release (FMAC)

* Datasheet: http://www.cypress.com/documentation/datasheets/cyw43364-single-chip-ieee-80211-bgn-macbasebandradio

* Cypress tar-ball based on: https://community.cypress.com/docs/DOC-15932

* The version of the chip used on the MangOH Yellow is from USI which incorporates
	both Bluetooth & Wifi in a SIP package. The Cypress/USI chip is only available
	on MangOH Yellow.

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
	Thus, we picked the next interface. This is not ideal as a tie-in to mdev and
	device naming should exist.

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

* It seems that to test with Legato one needs to setup a fake acces point such that
    one can set the interface to wlan1. Note that MangOH Red has wlan1 used
    by the MT7697 Wifi chip which is not present on the MangOH Yellow. AP_REF refers
    to the AP reference found from scan. These are the steps:
  * To have the Cypress Wifi start via init.d properly (i.e. ifup scripts) you need to add
      following lines to "/etc/network/interfaces":
      
      iface wlan1 inet manual
      pre-up /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh start
      post-down /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh stop
    
  * Please apply the Legato Patches as mentioned under:
     https://github.com/mangOH/mangOH/wiki/mangOH-Red-mt7697-WiFi
  * /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh init
  * wifi client create mangOH
  * wifi client setinterface 0x10000001 wlan1
  * wifi client start
  * wifi client setdriver 0x10000001 wnl80211
  * wifi client scan
  * wifi client setsecurityproto AP_REF 3
  * wifi client setpassphrase AP_REF
  * wifi client connect AP_REF
  * /sbin/udhcpc -R -b -i wlan1
  * ping www.google.com

* For DCS (Data Connection Service) testing please do the following steps:
  * Please apply the Legato Patches as mentioned under:
     https://github.com/mangOH/mangOH/wiki/mangOH-Red-mt7697-WiFi
  * Add the following lines to /etc/init.d/startlegato.sh on the target filesystem:
	```
        if [ -e "/legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh" ]
        then
            /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh init
        fi
	```
    between "mount -o bind $LEGATO_MNT /legato" and "test -x $LEGATO_START && $LEGATO_START"

  * config set dataConnectionService:/wifi/SSID "SSID of your WiFi" string
  * config set dataConnectionService:/wifi/passphrase "password" string
  * config set dataConnectionService:/wifi/interface wlan1 string
  * config set dataConnectionService:/wifi/wpaSupplicantDriver wext string
  * config set dataConnectionService:/wifi/secProtocol 3 int
  * Follow Legato instructions on starting up and configuring DCS.
  * ping www.google.com

* Testing outside of Legato was done simply with the following commands (no
   Legato Patches are needed):
  * /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh init
  * /legato/systems/current/modules/files/brcmutil/etc/init.d/cywifi.sh start
  * wpa_passphrase YOUR_SSID >> /etc/wpa_supplicant.conf (this will ask you for a password).
  * wpa_supplicant -B -Dnl80211 -iwlan1 -c /etc/wpa_supplicant.conf
  * /sbin/udhcpc -R -b -i wlan1
  * ping www.google.com
