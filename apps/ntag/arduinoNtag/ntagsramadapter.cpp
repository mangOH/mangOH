#include "ntagsramadapter.h"
#include "Ndef.h"

NtagSramAdapter::NtagSramAdapter(Ntag* ntag)
{
    _ntag=ntag;
}

bool NtagSramAdapter::begin(){
    NtagAdapter::begin();
    //Mirror SRAM to bottom of USERMEM
    //  the PN532 reader will read SRAM instead of EEPROM
    //  the advantage is that the same driver code for the PN532 reader can be used as for reading RFID-cards.
    //  the disadvantage is that the tag has to poll to over I²C to check if the memory is still locked to the RF-side.
    //Set FD_pin to function as handshake signal
    if((!_ntag->setSramMirrorRf(true, 0x01)) || (!_ntag->setFd_ReaderHandshake())){
        LE_INFO("Can't initialize tag");
        return false;
    }
    return true;
}

bool NtagSramAdapter::write(NdefMessage& message, unsigned int uiTimeout){
    if(!waitUntilRfDone(uiTimeout))
    {
        return false;
    }
    byte encoded[message.getEncodedSize()];
    message.encode(encoded);
    if(3 + sizeof(encoded) > SRAM_SIZE){
        return false;
    }
    byte buffer[3 + sizeof(encoded)];
    memset(buffer, 0, sizeof(buffer));
    buffer[0] = MESSAGE_TYPE_NDEF;
    buffer[1] = sizeof(encoded);
    memcpy(&buffer[2], encoded, sizeof(encoded));
    buffer[2+sizeof(encoded)] = 0xFE; // terminator
    _ntag->writeSram(0,buffer,3 + sizeof(encoded));
    _ntag->setLastNdefBlock();
    _ntag->releaseI2c();
    return true;
//    for(int i=0;i<sizeof(buffer);i++){
//        LE_INFO(buffer[i], HEX);LE_INFO(" ");
//        if((i+1)%8==0)LE_INFO();
//    }
}

NfcTag NtagSramAdapter::read(unsigned int uiTimeOut){
    int messageStartIndex = 0;
    int messageLength = 0;
    byte buffer[SRAM_SIZE];

    if(!waitUntilRfDone(uiTimeOut)){
        return NfcTag(uid,UID_LENGTH,"NOT READY2");
    }
    if(!_ntag->readSram(0,buffer,SRAM_SIZE)){
        return NfcTag(uid,UID_LENGTH,"ERROR");
    }
    _ntag->releaseI2c();
//    for(int i=0;i<SRAM_SIZE;i++){
//        LE_INFO(buffer[i], HEX);LE_INFO(" ");
//        if((i+1)%8==0)LE_INFO();
//    }
//    LE_INFO();
    if (!decodeTlv(buffer, messageLength, messageStartIndex)) {
        return NfcTag(uid, UID_LENGTH, "ERROR");
    }
    return NfcTag(uid, UID_LENGTH, "NTAG", &buffer[messageStartIndex], messageLength);
}


