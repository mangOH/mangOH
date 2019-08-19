#include <ntagsramadapter.h>
#include "Arduino.h"
#define HARDI2C
#include <Wire.h>

Ntag ntag(Ntag::NTAG_I2C_2K,7,9);
NtagSramAdapter ntagAdapter(&ntag);

void setup(){
    LE_INFO("start");
    if(!ntag.begin()){
        LE_INFO("Can't find ntag");
    }
    getSerialNumber();
    testUserMem();
    testRegisterAccess();
    testSramMirror();
    testSram();
}

COMPONENT_INIT
{
        setup();
}


void testWriteAdapter(){
    NdefMessage message = NdefMessage();
    message.addUriRecord("http://www.google.be");
    if(ntagAdapter.write(message)){
        LE_INFO("Message written to tag.");
    }
}

void testSram(){
    byte data[16];
    LE_INFO("Reading SRAM block 0xF8");
    if(ntag.readSram(0,data,16)){
        showBlockInHex(data,16);
    }
    for(byte i=0;i<16;i++){
        data[i]=0xF0 | i;
    }
    LE_INFO("Writing dummy data to SRAM block 0xF8");
    if(!ntag.writeSram(0,data,16)){
        return;
    }
    LE_INFO("Reading SRAM block 0xF8 again");
    if(ntag.readSram(0,data,16)){
        showBlockInHex(data,16);
    }
}

void testSramMirror(){
    byte readeeprom[16];
    byte data;

    if(!ntag.setSramMirrorRf(false,1))return;
    LE_INFO("\nReading memory block 1, no mirroring of SRAM");
    if(ntag.readEeprom(0,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
    LE_INFO("\nReading SRAM block 1");
    if(ntag.readSram(0,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
    if(!ntag.setSramMirrorRf(true,1))return;
    if(ntag.readRegister(Ntag::NC_REG,data))
    	LE_INFO("NC_REG: %x", data);
    LE_INFO("Use an NFC-reader to verify that the SRAM has been mapped to the memory area that the reader will access by default.");
}

void testRegisterAccess(){
    byte data;
    if(ntag.readRegister(Ntag::NC_REG,data))
	LE_INFO("Reading NC_REG Success, Data: %x", data);
    else
	LE_INFO("Reading NC_REG Failed");
    if(ntag.writeRegister(Ntag::NC_REG,0x0C,0x0A))
	LE_INFO("Writing NC_REG Success, Data Written: 0x0C,0x0A");
    else
	LE_INFO("Writing NC_REG Failed");
    if(ntag.readRegister(Ntag::NC_REG,data))
	LE_INFO("Reading NC_REG Success, Data: %x", data);
    else
	LE_INFO("Reading NC_REG Failed");
}

void getSerialNumber(){
    uint32_t bin_sz = ntag.getUidLength()
    uint32_t char_sz  = (2 * bin_sz) + 1;
    byte* sn=(byte*)malloc(bin_sz);
    char cp = (char *) malloc(char_sz);
    if(ntag.getUid(sn,bin_sz))
    {
	if(le_hex_BinaryToString(sn,bin_sz,cp,char_sz) != -1)
            LE_INFO("Serial number of the tag is: %s", cp);
    	else
	    LE_INFO("getSerialNumber failed in call to le_hex_BinaryToString");
    }
    free(sn);
    free(cp);
}

void testUserMem(){
    byte eepromdata[2*16];
    byte readeeprom[16];

    for(byte i=0;i<2*16;i++){
        eepromdata[i]=0x80 | i;
    }

    LE_INFO("Writing block 1");
    if(!ntag.writeEeprom(0,eepromdata,16)){
        LE_INFO("Write block 1 failed");
    }
    LE_INFO("Writing block 2");
    if(!ntag.writeEeprom(16,eepromdata+16,16)){
        LE_INFO("Write block 2 failed");
    }
    LE_INFO("\nReading memory block 1");
    if(ntag.readEeprom(0,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
    LE_INFO("Reading memory block 2");
    if(ntag.readEeprom(16,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
    LE_INFO("Reading bytes 10 to 20: partly block 1, partly block 2");
    if(ntag.readEeprom(10,readeeprom,10)){
        showBlockInHex(readeeprom,10);
    }
    LE_INFO("Writing byte 15 to 20: partly block 1, partly block 2");
    for(byte i=0;i<6;i++){
        eepromdata[i]=0x70 | i;
    }
    if(ntag.writeEeprom(15,eepromdata,6)){
        LE_INFO("Write success");
    }
    LE_INFO("\nReading memory block 1");
    if(ntag.readEeprom(0,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
    LE_INFO("Reading memory block 2");
    if(ntag.readEeprom(16,readeeprom,16)){
        showBlockInHex(readeeprom,16);
    }
}

void showBlockInHex(byte* data, byte size){
    uint32_t char_sz  = (2 * bin_sz) + 1;
    char cp = (char *) malloc(char_sz);

    if(le_hex_BinaryToString(data,size,cp,char_sz) != -1)
	LE_INFO("%s", cp);
    else
	LE_INFO("showBlockInHex failed in call to le_hex_BinaryToString");
}
