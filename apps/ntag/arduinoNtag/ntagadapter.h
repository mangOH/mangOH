#ifndef NTAGADAPTER_H
#define NTAGADAPTER_H

#include "ntag.h"
#include "NDEF/NfcTag.h"


class NtagAdapter
{
public:
    virtual bool begin();
    virtual bool write(NdefMessage& message, unsigned int uiTimeout=0)=0;
    virtual NfcTag read(unsigned int uiTimeOut=0)=0;
    bool readerPresent(unsigned long timeout=0);
    bool rfBusy();
    // erase tag by writing an empty NDEF record
    bool erase();
    // format a tag as NDEF
    bool format();
    // reset tag back to factory state
    bool clean();
    bool getUid(byte *uidin, unsigned int uidLength);
    byte getUidLength();
protected:
    static const byte NTAG_PAGE_SIZE=4;
    static const byte NTAG_BLOCK_SIZE=16;
    static const byte NTAG_DATA_START_BLOCK=1;
    static const byte MESSAGE_TYPE_NDEF=3;//TLV Block type
    Ntag* _ntag;
    bool waitUntilRfDone(unsigned int uiTimeOut);
    static const byte UID_LENGTH=7;
    bool decodeTlv(byte *data, int &messageLength, int &messageStartIndex);
    int getNdefStartIndex(byte *data);
    byte uid[UID_LENGTH];  // Buffer to store the returned UID
};

#endif // NTAGADAPTER_H
