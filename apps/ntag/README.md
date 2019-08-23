## NFC

* Datasheet: https://www.nxp.com/docs/en/data-sheet/NT3H2111_2211.pdf

* From: git://github.com/LieBtrau/arduino-ntag

* This code is for the Arduino on an older version of the NTAG and
	includes much NDEF code for the MifareClassic and MifareUltralight
	tags.

* Porting details
  * We have modified all "*.ino" files to "*.cpp" as they are C++ files generated
	by the NXP IDE.
  * We have not included any of the Mifare src files in the build.
  * We have deleted many directories useful to the NFC controller & the Arduino only.
  * Started using a Arduino Wire interface for Pi from: https://github.com/mhct/wire-linux.
	This library had too many problems in communicating with the ntag, such that
	we moved to combined transactions across I2c.
	Thus, this is mostly a rewrite to get arduino-ntag working.
	Note that we had to write separate I2C code for register
	vs. block reading - this is due to the NTAG I2c differences
	from most I2c devices.
  * Had to port some core Arduino libraries to get things working.
  * Much of arduino_types.c (bad name - it is more than types) is from git://git.drogon.net/wiringPi
	i.e. Gordon Henderson's WiringPi library
  * We have Legatofied only the Arduino code that we used, others are not Legatofied.
  * Much of the code mixes PAGE_SIZE code with BLOCK_SIZE code and things
	do break because of that. We have corrected the code that we have ported, but
	not any of the non-ported code. Aside: PAGE_SIZE is for the NFC interface
	and BLOCK_SIZE is for the I2C interface.
  * Ntag::read was re-written - it was trying to do non BLOCK_SIZE reads.
  * The isUnformatted member function was made public & corrected - the Capability Container
	for the NT3H2111_2211 is all zeros before formatting. Maybe Mifare tags were all 0xFF's.

* Examples - most are from the original code-base mostly Legatofied
  * ReadTag - to read the tag as Hex characters
  * WriteTag - currently hard-coded, should allow passing in an NDEF record, also append is needed.
  * EraseTag - to erase the tag - leaves an empty NDEF record
  * CleanTag - Prints an error though it has reset tag to a factory like state. Note CC is OTP still.
  * ReadTagExtended - to read the NDEF records in the tag as strings
  * WriteTagMultipleRecords - ditto to WriteTag, except allow multiple NDEFs.
  * ntag - command line oriented i/f to the tag - allows all the above ops. Adds the following:
	* text - write a text message NDEF record to the tag - rest of command-line in quotes
        * pushhubmemory - push the tag memory to the datahub as strings - need to add sram memory.
        * pushndef - push the ndefs to the datahub as a string array
  * NtagDhubIf app/service exists to trigger on NtagDhubIf/writeNDEF to write a text only
	NDEF to the tag. This service also pushes up to ntag/ndef on Field Detect (FD).
	We do need some minor debouncing sometimes - seems more for users who are not
	so still during FD.

* Standards - the NFC standards require at least 5K USD for membership and access. Thus, it was quite
	challenging to modify the NDEF code. If you have access to the standard, code pull requests
	with changes will be welcomed.

* What's Missing
  * Formatting a tag in the beginning - please perform from your phone for now - see below.
	Code exists in Don Coleman's NDEF library but it is specific to MifareClassic
	type of ntag and not our NT3H2111_2211.

* Starting up
  * Need an Android phone and download the "NTAG I2C Demo" app. Recently, NXP has added support
	for the iPhone at: https://apps.apple.com/us/app/nfc-taginfo-by-nxp/id1246143596
  * Format the tag - in the Android NTAG Demo app do Reset Tag, but be very careful to not
	create a password - it defaults if by accident you get to that screen (and then
	the tag is almost brick'ed).  The Android Tagwriter also works - but a similar issue
	of defaulting password exists, so be careful. Another alternative, is to use NXP's
	reader/writer controller (we have not tried).
  * Run any of the examples.
  * We had so many problems with I2CDemo or TagWriter apps setting the password auth.
	setting, we always clear the passwd auth setting in pushhubmemory to be safe.
	Please comment out code in ntag_main.cpp if you need to use passwords.

* For license reasons we have split into 4 areas:
  * arduinoNtag - GPL.
  * arduinoLibs - LGPL.
  * ntagCmdApp  - our app code.
  * NtagDhubIf  - our service code for dhub triggers to the edge & FD interrupts for pushing up to dhub.

* TODO - Alas, so much needs to be done
  * Needs much more testing - please provide feedback/pull requests.
  * We cannot parse the multiple nested NDEF records inside the Smart Poster NDEF record
	that the NTAG Demo App writes on the Reset operation - so we print "PARSEFAIL Type: Sp"
  * NFC API needs to be built, either in an remote memory access scheme or a Legato API
	needs to be built for access. For a starter command-line is fine.
  * All the MifareClassic (think 2014) code needs to upgraded for the NT3H2111_2211
  * One could upgrade to the ntag one-level up that allows X.509/PKI.
  * The Field Detect app is in apps/NtagDhubIf - that app also allows writes from dhub
	(i.e. cloud or Legato).
  * I'm sure bugs exist in the code as we put things together from Arduino, Raspberry Pi, ...
	Please submit fixes as pull requests.
  * We need to incorporate a Format op for the ntag as it easy to mistakenly add a password
	during the NTAG I2C Demo Reset Tag used as our normal Format tag operation.
	Code commented out to clear the password auth setting - maybe an op should exist.

