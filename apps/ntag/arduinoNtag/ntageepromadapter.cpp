//[BSD License](https://github.com/don/Ndef/blob/master/LICENSE.txt) (c) 2013-2014, Don Coleman

#include "ntageepromadapter.h"

NtagEepromAdapter::NtagEepromAdapter(Ntag* ntag)
{
    _ntag=ntag;
}

bool NtagEepromAdapter::begin()
{
    NtagAdapter::begin();
    // LE_INFO("Calling setSramMirrorRf");
    if((!_ntag->setSramMirrorRf(false, 0)) || (!_ntag->setFd_ReaderHandshake())){
        LE_INFO("Can't initialize tag");
        return false;
    }
    return true;
}

bool NtagEepromAdapter::write(NdefMessage& m, unsigned int uiTimeout){
    if(!waitUntilRfDone(uiTimeout))
    {
        return false;
    }
    if(!readCapabilityContainer())
    {
        return false;
    }

    if (isUnformatted())
    {
        LE_INFO("WARNING: Tag is not formatted.");
        return false;
    }
    messageLength  = m.getEncodedSize();
    ndefStartIndex = messageLength < 0xFF ? 2 : 4;
    calculateBufferSize();

    if(bufferSize>tagCapacity) {
#ifdef MIFARE_ULTRALIGHT_DEBUG
        LE_INFO("Encoded Message length exceeded tag Capacity ");LE_INFO(tagCapacity);
#endif
        return false;
    }

    uint8_t encoded[bufferSize];

    // Set message size.
    encoded[0] = 0x3;
    if (messageLength < 0xFF)
    {
        encoded[1] = messageLength;
    }
    else
    {
        encoded[1] = 0xFF;
        encoded[2] = ((messageLength >> 8) & 0xFF);
        encoded[3] = (messageLength & 0xFF);
    }
    m.encode(encoded+ndefStartIndex);
    // this is always at least 1 byte copy because of terminator.
    memset(encoded+ndefStartIndex+messageLength,0,bufferSize-ndefStartIndex-messageLength);
    encoded[ndefStartIndex+messageLength] = 0xFE; // terminator

#ifdef MIFARE_ULTRALIGHT_DEBUG
    LE_INFO("messageLength ");LE_INFO(messageLength);
    LE_INFO("Tag Capacity ");LE_INFO(tagCapacity);
    nfc->PrintHex(encoded,bufferSize);
#endif

    _ntag->writeEeprom(0,encoded,bufferSize);
    _ntag->setLastNdefBlock();
    _ntag->releaseI2c();
    return true;
    //    for(int i=0;i<sizeof(buffer);i++){
    //        LE_INFO(buffer[i], HEX);LE_INFO(" ");
    //        if((i+1)%8==0)LE_INFO();
    //    }
}

NfcTag NtagEepromAdapter::read(unsigned int uiTimeOut)
{
    bool success;
    success = isUnformatted();
    // LE_INFO("isUnformatted returned");
    if (success == true) {
        LE_INFO("WARNING: Tag is not formatted.");
        return NfcTag(uid, UID_LENGTH, NFC_FORUM_TAG_TYPE_2);
    }

    // LE_INFO("Calling readCapabilityContainer");
    if(!readCapabilityContainer())
    {
        return NfcTag(uid, UID_LENGTH, NFC_FORUM_TAG_TYPE_2);;
    }
    // LE_INFO("readCapabilityContainer returned");
    findNdefMessage();
    // LE_INFO("findNdefMessage returned");
    calculateBufferSize();
    // LE_INFO("calculateBufferSize returned");

    if (messageLength == 0) { // data is 0x44 0x03 0x00 0xFE
        NdefMessage message = NdefMessage();
        message.addEmptyRecord();
        return NfcTag(uid, UID_LENGTH, NFC_FORUM_TAG_TYPE_2, message);
    }

    byte buffer[bufferSize];
    _ntag->readEeprom(0,buffer, bufferSize);
    NdefMessage ndefMessage = NdefMessage(&buffer[ndefStartIndex], messageLength);
    return NfcTag(uid, UID_LENGTH, NFC_FORUM_TAG_TYPE_2, ndefMessage);
}

// Mifare Ultralight can't be reset to factory state
// zero out tag data like the NXP Tag Write Android application
bool NtagEepromAdapter::clean()
{
    // LE_INFO("NtagEepromAdapter::clean called");
    if(!readCapabilityContainer()) {
        return false;
    }

    byte blocks = (tagCapacity / NTAG_BLOCK_SIZE);
    LE_INFO("NtagEepromAdapter::clean blocks: %x tagCapacity: %x", blocks, tagCapacity);

    // factory tags have 0xFF, but OTP-CC blocks have already been set so we use 0x00
    byte data[16];
    memset(data,0x00,sizeof(data));

    for (int i = 0; i < blocks; i++) {
        //#ifdef MIFARE_ULTRALIGHT_DEBUG
	//NT3H2111_2211 does not allow reads on these blocks
	// and we do not want overwrite the config registers at 0x3A
        if(i > 0x38 && i < 0x3F)
            continue;
        LE_INFO("Writing block %X - ", i+1);
        //PrintHex(data, NTAG_BLOCK_SIZE);
        //#endif
	// SWI - this should be a byte address rather than a block number
        if (!_ntag->writeEeprom(i * NTAG_BLOCK_SIZE,data,NTAG_BLOCK_SIZE)) {
            return false;
        }
    }
    return true;
}

bool NtagEepromAdapter::erase()
{
    NdefMessage message = NdefMessage();
    message.addEmptyRecord();
    return write(message);
}


bool NtagEepromAdapter::isUnformatted()
{
    const byte PAGE_4 = 0;//page 4 is base address of EEPROM from I²C perspective
    byte data[NTAG_PAGE_SIZE];
    bool success = _ntag->readEeprom(PAGE_4, data, NTAG_PAGE_SIZE);
    /*LE_INFO("In NtagEepromAdapter::isUnformatted read back:");
    PrintHex(data,NTAG_PAGE_SIZE); */

    if (success) {
	/* LE_INFO("_ntag->readEeprom returned success: %x, data ptr: %p", success, (void *) &data[0]);
	LE_INFO("data[0]: %X data[1]: %X data[2]: %X data[3]: %X",
		data[0], data[1], data[2], data[3]);  */
		
        return (data[0] == 0x0 && data[1] == 0x0 && data[2] == 0x0 && data[3] == 0x0);
    }
    else {
        LE_INFO("Error. Failed read page 4");
        return false;
    }
    //LE_INFO("Return from isUnformatted");
}

// page 3 has tag capabilities
bool NtagEepromAdapter::readCapabilityContainer()
{
    byte data[4];
    if (_ntag->getCapabilityContainer(data))
    {
        //http://apps4android.org/nfc-specifications/NFCForum-TS-Type-2-Tag_1.1.pdf
        if(data[0]!=0xE1)
        {
            return false;   //magic number
        }
        //NT3H1101 return 0x6D for data[2], which leads to 872 databytes, not 888.
        tagCapacity = data[2] * 8;
#ifdef MIFARE_ULTRALIGHT_DEBUG
        LE_INFO("Tag capacity "));LE_INFO(tagCapacity);LE_INFO(F(" bytes");
#endif

        // TODO future versions should get lock information
    }
    return true;
}

// buffer is larger than the message, need to handle some data before and after
// message and need to ensure we read full pages
// SWI Buffer Size for I2c needs to be calculated based on Blocks
void NtagEepromAdapter::calculateBufferSize()
{
    // TLV terminator 0xFE is 1 byte
    bufferSize = messageLength + ndefStartIndex + 1;

    if (bufferSize % NTAG_BLOCK_SIZE != 0)
    {
        // buffer must be an increment of page size
        bufferSize = ((bufferSize / NTAG_BLOCK_SIZE) + 1) * NTAG_BLOCK_SIZE;
    }
    // LE_INFO("bufferSize for NDEF messages: %X", bufferSize);
}

// read enough of the message to find the ndef message length
void NtagEepromAdapter::findNdefMessage()
{
    byte data[16]; // 4 pages

    // the nxp read command reads 4 pages
    if (_ntag->readEeprom(0,data,16))
    {
        if (data[0] == 0x03)
        {
            messageLength = data[1];
            ndefStartIndex = 2;
        }
        else if (data[5] == 0x3) // page 5 byte 1
        {
            // TODO should really read the lock control TLV to ensure byte[5] is correct
            messageLength = data[6];
            ndefStartIndex = 7;
        }
    }

    //#ifdef MIFARE_ULTRALIGHT_DEBUG
    // LE_INFO("messageLength: %X ndefStartIndex: %X", messageLength, ndefStartIndex);
    //#endif
}


