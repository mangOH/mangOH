// Receive a NDEF message from a Peer
// Requires SPI. Tested with Seeed Studio NFC Shield v2

#include "SPI.h"
#include "PN532_SPI.h"
#include "snep.h"
#include "NdefMessage.h"

PN532_SPI pn532spi(SPI, 10);
SNEP nfc(pn532spi);
uint8_t ndefBuf[128];

void setup() {
    LE_INFO("NFC Peer to Peer Example - Receive Message");
}

void loop() {
    LE_INFO("Waiting for message from Peer");
    int msgSize = nfc.read(ndefBuf, sizeof(ndefBuf));
    if (msgSize > 0) {
        NdefMessage msg  = NdefMessage(ndefBuf, msgSize);
        msg.print();
        LE_INFO("\nSuccess");
    } else {
        LE_INFO("Failed");
    }
    delay(3000);
}

