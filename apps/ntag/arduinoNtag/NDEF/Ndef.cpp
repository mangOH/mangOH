#include "legato.h"
#include "Ndef.h"

// Borrowed from Adafruit_NFCShield_I2C
void PrintHex(const byte * data, const long numBytes)
{
  PrintHexChar(data, numBytes);
}

// Borrowed from Adafruit_NFCShield_I2C
void PrintHexChar(const byte * data, const long numBytes)
{
    uint32_t char_sz  = (2 * numBytes) + 1;
    char *cp = (char *) malloc(char_sz);

    if(le_hex_BinaryToString(data,numBytes,cp,char_sz) != -1)
        LE_INFO("%s", cp);
    else
        LE_INFO("PrintHexChar failed in call to le_hex_BinaryToString");
}

// Note if buffer % blockSize != 0, last block will not be written
void DumpHex(const byte * data, const long numBytes, const unsigned int blockSize)
{
    unsigned int i;
    for (i = 0; i < (numBytes / blockSize); i++)
    {
        PrintHexChar(data, blockSize);
        data += blockSize;
    }
}
