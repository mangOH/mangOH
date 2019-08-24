// Clean resets a tag back to factory-like state
// For Mifare Classic, tag is zero'd and reformatted as Mifare Classic
// For Mifare Ultralight, tags is zero'd and left empty

#include "ntagDefs.h"

/* SWI mangOH Yellow has the 2K Ntag */
static Ntag ntag(Ntag::NTAG_I2C_2K,2,5);
static NtagEepromAdapter ntagAdapter(&ntag);

void CleanTag(void)
{
    ntagAdapter.begin();
    bool success = ntagAdapter.clean();
    if (success)
    {
        LE_INFO("\nSuccess, tag restored to factory state.");
    } else
    {
        LE_INFO("\nError, unable to clean tag.");
    }
}

