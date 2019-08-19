#include "ntagadapter.h"

bool NtagAdapter::begin()
{
    if(_ntag->begin() == false)
	return false;
    _ntag->getUid(uid, sizeof(uid));
    return true;
}

bool NtagAdapter::readerPresent(unsigned long timeout)
{
    unsigned long startTime=millis();
    do
    {
        if(_ntag->isReaderPresent())
        {
            return true;
        }
    }while(millis()<startTime+timeout);

    return false;
}

bool NtagAdapter::waitUntilRfDone(unsigned int uiTimeOut)
{
    if(uiTimeOut>0)
    {
        unsigned long ulStartTime=millis();
        while(millis() < ulStartTime+uiTimeOut)
        {
            if(!(_ntag->isRfBusy()))
            {
                return true;
            }
        }
    }
    return !(_ntag->isRfBusy());
}

bool NtagAdapter::rfBusy(){
    return _ntag->isRfBusy();
}

// Decode the NDEF data length from the Mifare TLV
// Leading null TLVs (0x0) are skipped
// Assuming T & L of TLV will be in the first block
// messageLength and messageStartIndex written to the parameters
// success or failure status is returned
//
// { 0x3, LENGTH }
bool NtagAdapter::decodeTlv(byte *data, int &messageLength, int &messageStartIndex)
{
    int i = getNdefStartIndex(data);

    if (i < 0 || data[i] != 0x3)
    {
        LE_INFO("Error. Can't decode message length.");
        return false;
    }
    else
    {
        messageLength = data[i+1];
        messageStartIndex = i + 2;
    }

    return true;
}

// skip null tlvs (0x0) before the real message
// technically unlimited null tlvs, but we assume
// T & L of TLV in the first block we read
int NtagAdapter::getNdefStartIndex(byte *data)
{

    for (int i = 0; i < 16; i++)
    {
        if (data[i] == 0x0)
        {
            // do nothing, skip
        }
        else if (data[i] == MESSAGE_TYPE_NDEF)
        {
            return i;
        }
        else
        {
            LE_INFO("Unknown TLV ");LE_INFO("%0x", data[i]);
            return -2;
        }
    }

    return -1;
}

byte NtagAdapter::getUidLength()
{
    return UID_LENGTH;
}

bool NtagAdapter::getUid(byte *uidin, unsigned int uidLength)
{
    memcpy(uidin, uid, UID_LENGTH < uidLength ? UID_LENGTH : uidLength);
    return true;
}

