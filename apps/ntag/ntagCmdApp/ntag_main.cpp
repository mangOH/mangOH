//-------------------------------------------------------------------------------------------------
/**
 * @file ntag_main.c
 *
 * NTAG Command line.
 *
 * Note that since the command is run in new context each time,
 * nothing can be saved between runs.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "ntagDefs.h"

// Data Hub resource path relative to the app's root.
#define TAG_FORMATTED       "formatted"
#define TAG_SERIAL_NUM      "serialNum"
#define TAG_NDEF            "ndef"
#define TAG_MEM_CC          "memory/cc"
#define TAG_MEM_CFG_REG     "memory/cfgReg"
#define TAG_MEM_SESSION_REG "memory/sessionReg"
#define TAG_MEM_EEPROM      "memory/eepromHex"
#define TAG_MEM_SRAM        "memory/sram"
#define JSON_EXAMPLE 	    "{\"TagType\",\"UID\",\"OPTIONAL_NDEF_RECORDS\"}"

// NTAG defines - all of these are privates in the arduino-ntag classes
#define NTAG_BLOCK_SIZE 16
#define UID_LENGTH      7        //non-portable depending on Tag Type
#define CC_LENGTH       4        //non-portable depending on Tag Type


//--------------------------------------------------------------------------------------------------
/**
 * General help message
 */
//--------------------------------------------------------------------------------------------------
static void Usage(void)
{
    printf("ntag command line usage\n"
        "ntag help\n"
        "To run ntag command do:\n"
        "ntag COMMAND\n"
        "\terase - erase the tag - leaves an empty NDEF record\n"
        "\tclean - reset a tag back to factory-like state\n"
        "\ttext - write a text message NDEF record to the tag - rest of command-line in quotes\n"
        "\twrite - write a NDEF record to the tag - hardcoded now\n"
        "\twriteall - write 3 NDEF records to the tag - hardcoded now\n"
        "\tread - read the tag\n"
        "\treadndef - read the NDEF records in the tag as strings\n"
        "\tpushhubmemory - push the tag memory to the datahub as strings\n"
        "\tpushndef - push the ndefs to the datahub as a string array\n"
        "\tformat - NOT IMPLEMENTED - use your Android phone\n"
        "\n");
}

void TextWrite(const char *text) {

      ntagAdapter.begin();
      NdefMessage message = NdefMessage();
      message.addTextRecord(text);

      if (!ntagAdapter.write(message)) {
        LE_INFO("NTAG Write failed of: %s", text);
        fprintf(stderr, "NTAG Write failed of: %s", text);
      }
}

void PushNdef(void) {
    char NdefJson[(2 * 2048) + 1];
    char *jsonp = NdefJson;
    char buf[BUFSIZ], buf2[BUFSIZ];

    ntagAdapter.begin();
    NfcTag tag = ntagAdapter.read();
    String TagType = tag.getTagType();
    TagType.toCharArray(buf, BUFSIZ);
    snprintf(buf2, BUFSIZ, "{\"TagType\": \"%s\",", buf);
    strncat(jsonp, buf2, BUFSIZ - 1);
    String UidString = tag.getUidString();
    UidString.toCharArray(buf, BUFSIZ);
    snprintf(buf2, BUFSIZ, "\"UID\": \"%s\"", buf);
    strncat(jsonp, buf2, BUFSIZ);

    if (tag.hasNdefMessage()) // every tag won't have a message
    {
        NdefMessage message = tag.getNdefMessage();
    	snprintf(buf2, BUFSIZ, ",\"RecordCount\": \"%d\",", (int) message.getRecordCount());
    	strncat(jsonp, buf2, BUFSIZ);

        // cycle through the records, printing some info from each
        int recordCount = message.getRecordCount();
        for (int i = 0; i < recordCount; i++)
        {
	    /* non-portable as the type field is based on typeLength - hack for now
	     * to prettier print the common case.
	     */
	    char type[9];   

    	    snprintf(buf2, BUFSIZ, "\"NDEF Record %d\": { ",  i+1);
    	    strncat(jsonp, buf2, BUFSIZ);
            NdefRecord record = message.getRecord(i);
            // NdefRecord record = message[i]; // alternate syntax

    	    snprintf(buf2, BUFSIZ, "\"TNF\": \"%x\",", record.getTnf());
    	    strncat(jsonp, buf2, BUFSIZ);
    	    String Type = record.getType();
    	    Type.toCharArray(buf, BUFSIZ);
    	    snprintf(buf2, BUFSIZ, "\"Type\": \"%s\",", buf); // will be "" for TNF_EMPTY
    	    strncat(jsonp, buf2, BUFSIZ);
	    strncpy(type, buf, BUFSIZ - 1);

            // The TNF and Type should be used to determine how your application processes the payload
            // There's no generic processing for the payload, it's returned as a byte[]
            int payloadLength = record.getPayloadLength();
            byte payload[payloadLength];
            record.getPayload(payload);

            // Print the Hex and Printable Characters
    	    uint32_t char_sz  = (2 * payloadLength) + 1;
    	    char *cp = (char *) malloc(char_sz);

    	    if(le_hex_BinaryToString(payload, payloadLength, cp, char_sz) != -1)
    	    	snprintf(buf2, BUFSIZ, "\"Hex Payload\": \"%s\",", cp);
    	    else
		snprintf(buf2, BUFSIZ, "le_hex_BinaryToString failed in parsing NDEF payload");

    	    strncat(jsonp, buf2, BUFSIZ);

            // Force the data into a String (might work depending on the content)
            // Real code should use smarter processing - TODO: useless code for URI & Text
            String payloadAsString = "";
            for (int c = 0; c < payloadLength; c++) {
                payloadAsString += (char)payload[c];
            }
	    payloadAsString.toCharArray(buf, BUFSIZ);

	    // Hack for Text & URI types - TODO: fix other common NDEF types
	    if(strcmp(type, "T") == 0) {
		payload[payloadLength] = '\0';
    	    	snprintf(buf2, BUFSIZ, "\"Payload (as String)\": \"%s\"", payload+3);
    	    	strncat(jsonp, buf2, BUFSIZ);
		//fprintf(stderr,"Text: %s\n", payload+3);
	    }
	    if(strcmp(type, "U") == 0) {
		payload[payloadLength] = '\0';
    	    	snprintf(buf2, BUFSIZ, "\"Payload (as String)\": \"%s\"", payload+1);
    	    	strncat(jsonp, buf2, BUFSIZ);
		//fprintf(stderr,"URI: %s\n", payload+1);
	    }
	    // NTAG I2c Demo Reset writes a smart poster with multiple nested NDEFS
	    // for which we need to add more processing to the parser - can't handle so bail
	    if(strcmp(type, "Sp") == 0) {
	    	snprintf(buf2, BUFSIZ, "\"PARSEFAIL Type\": \"%s\"}}", "Sp");
    	    	strncat(jsonp, buf2, BUFSIZ);
    		//fprintf(stderr,"JSON: %s\n", jsonp);
    		dhubIO_PushJson(TAG_NDEF, DHUBIO_NOW, jsonp);
		return;
	    }

            // id is probably blank and will return ""
            String uid = record.getId();
            if (uid != "") {
		uid.toCharArray(buf, BUFSIZ);
    	    	snprintf(buf2, BUFSIZ, ",\"ID\": \"%s\"", buf);
    	    	strncat(jsonp, buf2, BUFSIZ);
            }
	    if(i == (recordCount - 1))
	    	strncat(jsonp, "}", 1);   // End of all records
	    else
	    	strncat(jsonp, "},", 2);   // End of record
        }
    }
    strncat(jsonp, "}", 1);   // End of JSON
    //fprintf(stderr,"JSON: %s\n", jsonp);
    dhubIO_PushJson(TAG_NDEF, DHUBIO_NOW, jsonp);
}

void PushTagMemoryToDhub(void) {

    /* NTAG array for mangOH Yellow 2K ntag - could be smaller as
     * UID, SRAM & Register space are separate.
     */
    //byte ntagMemory[NTAG_SIZE];
    //byte *ntagp = ntagMemory;
    //int len;
    int i;
    byte uid[UID_LENGTH];
    char SerialNumber[(2 * UID_LENGTH) + 1];
    byte CC[CC_LENGTH];
    char CCHex[(2 * CC_LENGTH) + 1];
    char EepromHex[(2 * 2048) + 1];
    char *ntagp = EepromHex;
    byte blk[NTAG_BLOCK_SIZE];
    char blkHex[(2 * NTAG_BLOCK_SIZE) + 1];
    byte reg;
    bool noEepromMem = true;

    // If we can't read the UID pack it in
    if(!ntag.getUid(uid,UID_LENGTH)) {
        fprintf(stderr, "Failed in reading the UID\n");
	exit(1);
    }

    if(le_hex_BinaryToString(uid, UID_LENGTH, SerialNumber, sizeof(SerialNumber)) != -1)
        fprintf(stderr, "Serial number of the tag is: %s\n", SerialNumber);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString\n");

    dhubIO_PushString(TAG_SERIAL_NUM, DHUBIO_NOW, SerialNumber);

    // If the tag is unformatted then there is no memory to send to dhub
    if(ntagAdapter.isUnformatted() == true) {
        fprintf(stderr, "WARNING: Tag is not formatted.\n");
	dhubIO_PushBoolean(TAG_FORMATTED, DHUBIO_NOW, false);
	noEepromMem = false;
    }

    dhubIO_PushBoolean(TAG_FORMATTED, DHUBIO_NOW, true);

    // Let's read the Capability Container
    if (ntag.getCapabilityContainer(CC)) {
	// Should not reach here as isUnformatted should have bombed earlier
        if(CC[0]!=0xE1) {
            fprintf(stderr, "WARNING: Tag Format Marker of 0xE1 is not in fist byte of CC\n");
	    noEepromMem = false;
        }
        fprintf(stderr, "Tag capacity %d bytes\n", CC[2] * 8);
    }
    else {
        fprintf(stderr, "WARNING: Could not read Capability Container\n");
	exit(1);
    }

    if(le_hex_BinaryToString(CC, CC_LENGTH, CCHex, sizeof(CCHex)) != -1)
        fprintf(stderr, "CC of the tag is: %s\n", CCHex);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString of CCHex\n");

    dhubIO_PushString(TAG_MEM_CC, DHUBIO_NOW, CCHex);

    // Let's reset the password auth setting
    byte x[1] = {0xFF};
    if(!ntag.writeEeprom((0x37 * NTAG_BLOCK_SIZE) + 15, x, 1)) {
        fprintf(stderr, "Failed in clearing password auth setting\n");
	exit(1);
    }

/*
    // Clear the NFC transfer direction in the Config Registers
    if(!ntag.writeRegister(Ntag::NC_REG, 0x1, 0x1)) {
        fprintf(stderr, "Failed in writing Register: %d\n", (int) Ntag::NC_REG);
	exit(1);
    }

    // Let's read password auth setting
    if(!ntag.readEeprom(0x37 * NTAG_BLOCK_SIZE, blk, NTAG_BLOCK_SIZE * 2)) {
        fprintf(stderr, "Failed in reading password auth memory\n");
	exit(1);
    }

    if(le_hex_BinaryToString(blk, NTAG_BLOCK_SIZE * 2, blkHex, sizeof(blkHex)) != -1)
        fprintf(stderr, "password auth hex of the tag is: %s\n", blkHex);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString of password auth setting\n");
*/

    // Let's read config registers
    if(!ntag.readEeprom(0x39 * NTAG_BLOCK_SIZE, blk, NTAG_BLOCK_SIZE)) {
        fprintf(stderr, "Failed in reading config registers\n");
	exit(1);
    }

    if(le_hex_BinaryToString(blk, NTAG_BLOCK_SIZE, blkHex, sizeof(blkHex)) != -1)
        fprintf(stderr, "config registers of the tag is: %s\n", blkHex);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString of config registers\n");

    dhubIO_PushString(TAG_MEM_CFG_REG, DHUBIO_NOW, blkHex);

/*
    // Let's fix NFC transfer direction
    blk[Ntag::NC_REG] |= 0x01;
    if(!ntag.writeEeprom(0x39 * NTAG_BLOCK_SIZE, blk, NTAG_BLOCK_SIZE)) {
        fprintf(stderr, "Failed in writing config register for NC_REG\n");
	exit(1);
    }
*/

    if(noEepromMem == true) {
    	fprintf(stderr, "Eeprom hex of the tag is:\n");
    	// Note readEeprom tacks on one 1 block to the block number - thus -1 on all block numbers
    	for(i = 0 ; i < 0x7F ; i++) {
	    //NT3H2111_2211 does not allow reads on these blocks
	    if(i > 0x39 && i < 0x3F)
	    	continue;
	    fprintf(stderr, "Block %X:\t", 1 + i);
    	    if(!ntag.readEeprom(i * NTAG_BLOCK_SIZE, blk, NTAG_BLOCK_SIZE)) {
                fprintf(stderr, "Failed in reading block\n");
	        exit(1);
    	    }
    	    if(le_hex_BinaryToString(blk, NTAG_BLOCK_SIZE, blkHex, sizeof(blkHex)) != -1) {
                fprintf(stderr, "%s\n", blkHex);
	        strcpy(ntagp, blkHex);
	        ntagp += strlen(blkHex);
	    }
    	    else
                fprintf(stderr, "Failed in call to le_hex_BinaryToString of Eeprom hex\n");
    	}
    }

    dhubIO_PushString(TAG_MEM_EEPROM, DHUBIO_NOW, EepromHex);
/*
    // Let's read SRAM memory - for the NT3H2111_2211 blocks 0xF8 - 0xFB are sram
    len = (0xFB - 0xF8 + 0x1) * NTAG_BLOCK_SIZE ;
    if(!ntag.readSram(0, ntagp, len))
    {
        fprintf(stderr, "Failed in reading SRAM memory\n");
	exit(2);
    }

    if(le_hex_BinaryToString(ntagp, len, EepromHex, sizeof(EepromHex)) != -1)
        fprintf(stderr, "SRAM hex of the tag is: %s\n", EepromHex);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString\n");

    ntagp += len;
*/
    dhubIO_PushString(TAG_MEM_SRAM, DHUBIO_NOW, "NOT IMPLEMENTED");

    // Register reading is a byte based interface
    for(i = 0 ; i <= 6 ; i++) {
        if(!ntag.readRegister((Ntag::REGISTER_NR) i, reg)) {
            fprintf(stderr, "Failed in reading Register: %d\n", i);
	    exit(3);
        }
        else {
	    //fprintf(stderr, "Session Register %d\t value: %X\n", i, reg);
	    blk[i] = reg;
        }
    }

    if(le_hex_BinaryToString(blk, 6, blkHex, sizeof(blkHex)) != -1)
        fprintf(stderr, "Session Regs of the tag is: %s\n", blkHex);
    else
        fprintf(stderr, "Failed in call to le_hex_BinaryToString of Session Regs \n");

    dhubIO_PushString(TAG_MEM_SESSION_REG, DHUBIO_NOW, blkHex);
}

//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // calling ntag without arguments will print the Usage
    if (le_arg_NumArgs() < 1)
    {
        Usage();
        exit(EXIT_SUCCESS);
    }
    else
    {
        const char *commandPtr = le_arg_GetArg(0);

        if ((0 == strcmp(commandPtr, "help")) ||
            (0 == strcmp(commandPtr, "--help")) ||
            (0 == strcmp(commandPtr, "-h")))
        {
            Usage();
            exit(EXIT_SUCCESS);
        }
        else
        {
            if (strcmp(commandPtr, "readndef") == 0)
            {
                ReadTagExtended();
            }
            else if (strcmp(commandPtr, "clean") == 0)
            {
                CleanTag();
            }
            else if (strcmp(commandPtr, "writeall") == 0)
            {
                WriteTagMultipleRecords();
            }
            else if (strcmp(commandPtr, "write") == 0)
            {
                WriteTag();
            }
            else if (strcmp(commandPtr, "read") == 0)
            {
                ReadTag();
            }
            else if (strcmp(commandPtr, "erase") == 0)
            {
                EraseTag();
            }
            else if (strcmp(commandPtr, "pushhubmemory") == 0)
            {
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_FORMATTED, DHUBIO_DATA_TYPE_BOOLEAN, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_SERIAL_NUM, DHUBIO_DATA_TYPE_STRING, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_MEM_CC, DHUBIO_DATA_TYPE_STRING, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_MEM_CFG_REG, DHUBIO_DATA_TYPE_STRING, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_MEM_SESSION_REG, DHUBIO_DATA_TYPE_STRING, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_MEM_EEPROM, DHUBIO_DATA_TYPE_STRING, ""));
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_MEM_SRAM, DHUBIO_DATA_TYPE_STRING, ""));
                PushTagMemoryToDhub();
            }
	    else if (strcmp(commandPtr, "pushndef") == 0)
            {
                LE_ASSERT(LE_OK == dhubIO_CreateInput(TAG_NDEF, DHUBIO_DATA_TYPE_JSON, ""));
		dhubIO_SetJsonExample(TAG_NDEF, JSON_EXAMPLE);
                PushNdef();
            }
	    else if (strcmp(commandPtr, "text") == 0)
            {
                TextWrite(le_arg_GetArg(1));
            }
            else
            {
                Usage();
                exit(EXIT_SUCCESS);
            }
        }
    }
    exit(EXIT_SUCCESS);
}
