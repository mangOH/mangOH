// Formats a Mifare Classic tags as an NDEF tag
// This will fail if the tag is already formatted NDEF
// nfc.clean will turn a NDEF formatted Mifare Classic tag back to the Mifare Classic format

#if 0
#include <SPI.h>
#include <PN532_SPI.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_SPI pn532spi(SPI, 10);
NfcAdapter nfc = NfcAdapter(pn532spi);
#else

#include <Wire.h>
#include <PN532_I2C.h>
#include <PN532.h>
#include <NfcAdapter.h>

PN532_I2C pn532_i2c(Wire);
NfcAdapter nfc = NfcAdapter(pn532_i2c);
#endif

void setup(void) {
    LE_INFO("NDEF Formatter");
    nfc.begin();
}

void loop(void) {
    
    LE_INFO("\nPlace an unformatted Mifare Classic tag on the reader.");
    if (nfc.tagPresent()) {

        bool success = nfc.format();
        if (success) {
          LE_INFO("\nSuccess, tag formatted as NDEF.");
        } else {
          LE_INFO("\nFormat failed.");
        }

    }
    delay(5000);
}
