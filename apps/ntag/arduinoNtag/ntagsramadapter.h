#ifndef NTAGSRAMADAPTER_H
#define NTAGSRAMADAPTER_H

#include "ntagadapter.h"

class NtagSramAdapter : public NtagAdapter
{
public:
    NtagSramAdapter(Ntag* ntag);
    bool begin();
    bool write(NdefMessage& message, unsigned int uiTimeout=0);
    NfcTag read(unsigned int uiTimeOut);
private:
    static const byte SRAM_SIZE=64;
};

#endif // NTAGSRAMADAPTER_H
