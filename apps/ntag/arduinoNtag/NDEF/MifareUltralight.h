#ifndef MifareUltralight_h
#define MifareUltralight_h

#include <PN532.h>
#include <NfcTag.h>
#include <Ndef.h>

class MifareUltralight
{
    public:
        MifareUltralight(PN532& nfcShield);
        ~MifareUltralight();
        NfcTag read(byte *uid, unsigned int uidLength);
        bool write(NdefMessage& ndefMessage, byte *uid, unsigned int uidLength);
        bool clean();
    private:
        PN532* nfc;
        unsigned int tagCapacity;
        unsigned int messageLength;
        unsigned int bufferSize;
        unsigned int ndefStartIndex;
        bool isUnformatted();
        void readCapabilityContainer();
        void findNdefMessage();
        void calculateBufferSize();
};

#endif
