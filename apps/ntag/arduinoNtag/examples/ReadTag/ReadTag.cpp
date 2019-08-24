#include "ntagDefs.h"

/* SWI mangOH Yellow has the 2K Ntag */
static Ntag ntag(Ntag::NTAG_I2C_2K,2,5);
static NtagEepromAdapter ntagAdapter(&ntag);

void ReadTag(void) {
    // LE_INFO("NDEF Reader");
    ntagAdapter.begin();
    // LE_INFO("Calling NtagEepromAdapter::read");
    NfcTag tag = ntagAdapter.read();
    tag.print();
}

