// Erases a NFC tag by writing an empty NDEF message
// The NDEF-messages is wrapped in a TLV-block.
//  T: 03       : TLV Block Type = NDEF-message
//  L: 03       : Length of value field = 3 bytes
//  V: D0 00 00 : Value field
//      NDEF-message
//          Record header : D0 = 1101000b
//              bits 2-0    : TNF = type name field = 0 : empty record
//          Type length     : 00
//          Payload length  : 00
//  TLV2: 0xFE = terminator TLV

#include "ntagDefs.h"

/* SWI mangOH Yellow has the 2K Ntag */
static Ntag ntag(Ntag::NTAG_I2C_2K,2,5);
static NtagEepromAdapter ntagAdapter(&ntag);

void EraseTag(void)
{
    ntagAdapter.begin();
    if (ntagAdapter.erase())
    {
        LE_INFO("\nSuccess, tag contains an empty record.");
    } else
    {
        LE_INFO("\nUnable to erase tag.");
    }
}

