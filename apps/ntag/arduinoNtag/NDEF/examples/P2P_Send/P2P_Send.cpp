// Sends a NDEF Message to a Peer
// Requires SPI. Tested with Seeed Studio NFC Shield v2

#include "SPI.h"
#include "PN532_SPI.h"
#include "snep.h"
#include "NdefMessage.h"

PN532_SPI pn532spi(SPI, 10);
SNEP nfc(pn532spi);
uint8_t ndefBuf[128];

void setup() {
    LE_INFO("NFC Peer to Peer Example - Send Message");
}

void loop() {
    LE_INFO("Send a message to Peer");
    
    NdefMessage message = NdefMessage();
    message.addUriRecord("http://shop.oreilly.com/product/mobile/0636920021193.do");
    //message.addUriRecord("http://arduino.cc");
    //message.addUriRecord("https://github.com/don/NDEF");

    
    int messageSize = message.getEncodedSize();
    if (messageSize > sizeof(ndefBuf)) {
        LE_INFO("ndefBuf is too small");
        while (1) {
        }
    }

    message.encode(ndefBuf);
    if (0 >= nfc.write(ndefBuf, messageSize)) {
        LE_INFO("Failed");
    } else {
        LE_INFO("Success");
    }

    delay(3000);
}
