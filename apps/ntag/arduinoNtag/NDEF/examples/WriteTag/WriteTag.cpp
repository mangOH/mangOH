#if 1
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

void setup() {
      LE_INFO("NDEF Writer");
      nfc.begin();
}

void loop() {
    LE_INFO("\nPlace a formatted Mifare Classic or Ultralight NFC tag on the reader.");
    if (nfc.tagPresent()) {
        NdefMessage message = NdefMessage();
        message.addUriRecord("http://arduino.cc");

        bool success = nfc.write(message);
        if (success) {
          LE_INFO("Success. Try reading this tag with your phone.");        
        } else {
          LE_INFO("Write failed.");
        }
    }
    delay(5000);
}
