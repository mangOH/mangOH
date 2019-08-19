//[BSD License](https://github.com/don/Ndef/blob/master/LICENSE.txt) (c) 2013-2014, Don Coleman
#ifndef NTAGEEPROMADAPTER_H
#define NTAGEEPROMADAPTER_H
#include "ntagadapter.h"


class NtagEepromAdapter : public NtagAdapter
{
public:
    NtagEepromAdapter(Ntag* ntag);
    bool begin();
    bool write(NdefMessage& message, unsigned int uiTimeout=0);
    NfcTag read(unsigned int uiTimeOut=0);
    bool clean();
    bool erase();
    bool isUnformatted();
private:
    const char* NFC_FORUM_TAG_TYPE_2="NFC Forum Type 2";
    unsigned int tagCapacity;
    unsigned int messageLength;
    unsigned int bufferSize;
    unsigned int ndefStartIndex;
    bool readCapabilityContainer();
    void findNdefMessage();
    void calculateBufferSize();
};

#endif // NTAGEEPROMADAPTER_H
