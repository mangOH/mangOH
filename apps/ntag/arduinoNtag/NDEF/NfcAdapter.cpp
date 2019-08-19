#include <NfcAdapter.h>

NfcAdapter::NfcAdapter(PN532Interface &interface)
{
    shield = new PN532(interface);
}

NfcAdapter::~NfcAdapter(void)
{
    delete shield;
}

void NfcAdapter::begin(bool verbose)
{
    shield->begin();

    uint32_t versiondata = shield->getFirmwareVersion();

    if (! versiondata)
    {
        LE_INFO("Didn't find PN53x board");
        while (1); // halt
    }

    if (verbose)
    {
        LE_INFO("Found chip PN5"); LE_INFO((versiondata>>24) & 0xFF, HEX);
        LE_INFO("Firmware ver. "); LE_INFO((versiondata>>16) & 0xFF, DEC);
        LE_INFO('.'); LE_INFO((versiondata>>8) & 0xFF, DEC);
    }
    // configure board to read RFID tags
    shield->SAMConfig();
}

bool NfcAdapter::tagPresent(unsigned long timeout)
{
    uint8_t success;
    uidLength = 0;

    if (timeout == 0)
    {
        success = shield->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, (uint8_t*)&uidLength);
    }
    else
    {
        success = shield->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, (uint8_t*)&uidLength, timeout);
    }
    return success;
}

bool NfcAdapter::erase()
{
    bool success;
    NdefMessage message = NdefMessage();
    message.addEmptyRecord();
    return write(message);
}

bool NfcAdapter::format()
{
    bool success;
    if (uidLength == 4)
    {
        MifareClassic mifareClassic = MifareClassic(*shield);
        success = mifareClassic.formatNDEF(uid, uidLength);
    }
    else
    {
        LE_INFO("Unsupported Tag.");
        success = false;
    }
    return success;
}

bool NfcAdapter::clean()
{
    uint8_t type = guessTagType();

    if (type == TAG_TYPE_MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Cleaning Mifare Classic");
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield);
        return mifareClassic.formatMifare(uid, uidLength);
    }
    else if (type == TAG_TYPE_2)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Cleaning Mifare Ultralight");
        #endif
        MifareUltralight ultralight = MifareUltralight(*shield);
        return ultralight.clean();
    }
    else
    {
        LE_INFO("No driver for card type ");LE_INFO(type);
        return false;
    }

}


NfcTag NfcAdapter::read()
{
    uint8_t type = guessTagType();

    if (type == TAG_TYPE_MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Reading Mifare Classic");
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield);
        return mifareClassic.read(uid, uidLength);
    }
    else if (type == TAG_TYPE_2)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Reading Mifare Ultralight");
        #endif
        MifareUltralight ultralight = MifareUltralight(*shield);
        return ultralight.read(uid, uidLength);
    }
    else if (type == TAG_TYPE_UNKNOWN)
    {
        LE_INFO("Can not determine tag type");
        return NfcTag(uid, uidLength);
    }
    else
    {
        LE_INFO("No driver for card type ");LE_INFO(type);
        // TODO should set type here
        return NfcTag(uid, uidLength);
    }

}

bool NfcAdapter::write(NdefMessage& ndefMessage)
{
    bool success;
    uint8_t type = guessTagType();

    if (type == TAG_TYPE_MIFARE_CLASSIC)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Writing Mifare Classic");
        #endif
        MifareClassic mifareClassic = MifareClassic(*shield);
        success = mifareClassic.write(ndefMessage, uid, uidLength);
    }
    else if (type == TAG_TYPE_2)
    {
        #ifdef NDEF_DEBUG
        LE_INFO("Writing Mifare Ultralight");
        #endif
        MifareUltralight mifareUltralight = MifareUltralight(*shield);
        success = mifareUltralight.write(ndefMessage, uid, uidLength);
    }
    else if (type == TAG_TYPE_UNKNOWN)
    {
        LE_INFO("Can not determine tag type");
        success = false;
    }
    else
    {
        LE_INFO("No driver for card type ");LE_INFO(type);
        success = false;
    }

    return success;
}

// TODO this should return a Driver MifareClassic, MifareUltralight, Type 4, Unknown
// Guess Tag Type by looking at the ATQA and SAK values
// Need to follow spec for Card Identification. Maybe AN1303, AN1305 and ???
unsigned int NfcAdapter::guessTagType()
{

    // 4 byte id - Mifare Classic
    //  - ATQA 0x4 && SAK 0x8
    // 7 byte id
    //  - ATQA 0x44 && SAK 0x8 - Mifare Classic
    //  - ATQA 0x44 && SAK 0x0 - Mifare Ultralight NFC Forum Type 2
    //  - ATQA 0x344 && SAK 0x20 - NFC Forum Type 4

    if (uidLength == 4)
    {
        return TAG_TYPE_MIFARE_CLASSIC;
    }
    else
    {
        return TAG_TYPE_2;
    }
}
