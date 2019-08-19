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

void setup(void) {
  LE_INFO("NDEF Reader");
  nfc.begin();
}

void loop(void) {
  LE_INFO("\nScan a NFC tag\n");

  if (nfc.tagPresent())
  {
    NfcTag tag = nfc.read();
    LE_INFO(tag.getTagType());
    LE_INFO("UID: ");LE_INFO(tag.getUidString());

    if (tag.hasNdefMessage()) // every tag won't have a message
    {

      NdefMessage message = tag.getNdefMessage();
      LE_INFO("\nThis NFC Tag contains an NDEF Message with ");
      LE_INFO(message.getRecordCount());
      LE_INFO(" NDEF Record");
      if (message.getRecordCount() != 1) {
        LE_INFO("s");
      }
      LE_INFO(".");

      // cycle through the records, printing some info from each
      int recordCount = message.getRecordCount();
      for (int i = 0; i < recordCount; i++)
      {
        LE_INFO("\nNDEF Record ");LE_INFO(i+1);
        NdefRecord record = message.getRecord(i);
        // NdefRecord record = message[i]; // alternate syntax

        LE_INFO("  TNF: ");LE_INFO(record.getTnf());
        LE_INFO("  Type: ");LE_INFO(record.getType()); // will be "" for TNF_EMPTY

        // The TNF and Type should be used to determine how your application processes the payload
        // There's no generic processing for the payload, it's returned as a byte[]
        int payloadLength = record.getPayloadLength();
        byte payload[payloadLength];
        record.getPayload(payload);

        // Print the Hex and Printable Characters
        LE_INFO("  Payload (HEX): ");
        PrintHexChar(payload, payloadLength);

        // Force the data into a String (might work depending on the content)
        // Real code should use smarter processing
        String payloadAsString = "";
        for (int c = 0; c < payloadLength; c++) {
          payloadAsString += (char)payload[c];
        }
        LE_INFO("  Payload (as String): ");
        LE_INFO(payloadAsString);

        // id is probably blank and will return ""
        String uid = record.getId();
        if (uid != "") {
          LE_INFO("  ID: ");LE_INFO(uid);
        }
      }
    }
  }
  delay(3000);
}
