#include "ntagDefs.h"

/* SWI mangOH Yellow has the 2K Ntag */
static Ntag ntag(Ntag::NTAG_I2C_2K,2,5);
static NtagEepromAdapter ntagAdapter(&ntag);

void ReadTagExtended(void) {
    char buf[BUFSIZ];

    ntagAdapter.begin();
    NfcTag tag = ntagAdapter.read();
    String TagType = tag.getTagType();
    TagType.toCharArray(buf, BUFSIZ);
    LE_INFO("TagType: %s", buf);
    String UidString = tag.getUidString();
    UidString.toCharArray(buf, BUFSIZ);
    LE_INFO("UID: %s", buf);

    if (tag.hasNdefMessage()) // every tag won't have a message
    {

        NdefMessage message = tag.getNdefMessage();
        LE_INFO("This NFC Tag contains an NDEF Messages with RecordCount: %d",
		(int) message.getRecordCount());

        // cycle through the records, printing some info from each
        int recordCount = message.getRecordCount();
        for (int i = 0; i < recordCount; i++)
        {
            LE_INFO("NDEF Record %d", i+1);
            NdefRecord record = message.getRecord(i);
            // NdefRecord record = message[i]; // alternate syntax

            LE_INFO("TNF: %x", record.getTnf());
    	    String Type = record.getType();
    	    Type.toCharArray(buf, BUFSIZ);
            LE_INFO("Type: %s", buf); // will be "" for TNF_EMPTY

            // The TNF and Type should be used to determine how your application processes the payload
            // There's no generic processing for the payload, it's returned as a byte[]
            int payloadLength = record.getPayloadLength();
            byte payload[payloadLength];
            record.getPayload(payload);

            // Print the Hex and Printable Characters
            LE_INFO("Payload (as HEX): ");
            PrintHexChar(payload, payloadLength);

            // Force the data into a String (might work depending on the content)
            // Real code should use smarter processing
            String payloadAsString = "";
            for (int c = 0; c < payloadLength; c++) {
                payloadAsString += (char)payload[c];
            }
	    payloadAsString.toCharArray(buf, BUFSIZ);
            LE_INFO("Payload (as String): %s", buf);

            // id is probably blank and will return ""
            String uid = record.getId();
            if (uid != "") {
		uid.toCharArray(buf, BUFSIZ);
                LE_INFO("ID: %s", buf);
            }
        }
    }
}

