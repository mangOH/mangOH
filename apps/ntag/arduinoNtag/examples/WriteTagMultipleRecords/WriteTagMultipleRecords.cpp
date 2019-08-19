#include "ntagDefs.h"

void WriteTagMultipleRecords()
{
    ntagAdapter.begin();
    NdefMessage message = NdefMessage();
    message.addTextRecord("Hello mangOH Yellow!");
    message.addUriRecord("https://mangoh.io/");
    message.addTextRecord("Goodbye mangOH Yellow!");
    if (ntagAdapter.write(message))
    {
        LE_INFO("Success. Try reading this tag with your phone.");
    } else
    {
        LE_INFO("Write failed");
    }
}

