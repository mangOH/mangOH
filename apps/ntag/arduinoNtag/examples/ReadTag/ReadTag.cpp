#include "ntagDefs.h"

void ReadTag(void) {
    // LE_INFO("NDEF Reader");
    ntagAdapter.begin();
    // LE_INFO("Calling NtagEepromAdapter::read");
    NfcTag tag = ntagAdapter.read();
    tag.print();
}

