#include "ntagDefs.h"

void WriteTag()
{
      //LE_INFO("NDEF Writer");
      ntagAdapter.begin();
      NdefMessage message = NdefMessage();
      message.addUriRecord("https://mangoh.io/");

      if (ntagAdapter.write(message)) {
        LE_INFO("Success. Try reading this tag with your phone.");
      } else {
        LE_INFO("Write failed.");
      }
}

