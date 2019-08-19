#include "legato.h"
#include <NdefMessage.h>

NdefMessage::NdefMessage(void)
{
    _recordCount = 0;
}

NdefMessage::NdefMessage(const byte * data, const int numBytes)
{
    //#ifdef NDEF_DEBUG
    // LE_INFO("Decoding numBytes: %d", numBytes);
    // PrintHexChar(data, numBytes);
    //DumpHex(data, numBytes, 16);
    //#endif

    _recordCount = 0;

    int index = 0;

    while (index <= numBytes) {
        // decode tnf - first byte is tnf with bit flags
        // see the NFDEF spec for more info
        byte tnf_byte = data[index];
        //bool mb = ((tnf_byte & 0x80) != 0);
        bool me = ((tnf_byte & 0x40) != 0);
        //bool cf = ((tnf_byte & 0x20) != 0);
        bool sr = ((tnf_byte & 0x10) != 0);
        bool il = ((tnf_byte & 0x8) != 0);
        byte tnf = (tnf_byte & 0x7);

	// LE_INFO("me: %x sr: %x il: %x tnf:%x", me, sr, il, tnf);
        NdefRecord record = NdefRecord();
        record.setTnf(tnf);

        index++;
        int typeLength = data[index];
	// LE_INFO("typeLength: %d", typeLength);

        int payloadLength = 0;
        if (sr) {
            index++;
            payloadLength = data[index];
	    // LE_INFO("payloadLength: %d", payloadLength);
        }
        else {
            payloadLength =
		((0xFF & data[(index+1)]) << 24)
		| ((0xFF & data[(index+2)]) << 16)
		| ((0xFF & data[(index+3)]) << 8)
		| (0xFF & data[(index+4)]);
	    index+=4;
	    // LE_INFO("payloadLength: %d", payloadLength);
        }

        int idLength = 0;
        if (il) {
            index++;
            idLength = data[index];
	    // LE_INFO("idLength: %d", idLength);
        }

        index++;
        record.setType(&data[index], typeLength);
        index += typeLength;

        if (il) {
            record.setId(&data[index], idLength);
            index += idLength;
        }

        record.setPayload(&data[index], payloadLength);
        index += payloadLength;

        addRecord(record);

        if (me) break; // last message
	// LE_INFO("index: %d", index);
    }
    // LE_INFO("Return from NdefMessage");
}

NdefMessage::NdefMessage(const NdefMessage& rhs)
{

    _recordCount = rhs._recordCount;
    for (unsigned int i = 0; i < _recordCount; i++)
    {
        _records[i] = rhs._records[i];
    }

}

NdefMessage::~NdefMessage()
{
}

NdefMessage& NdefMessage::operator=(const NdefMessage& rhs)
{

    if (this != &rhs)
    {

        // delete existing records
        for (unsigned int i = 0; i < _recordCount; i++)
        {
            // TODO Dave: is this the right way to delete existing records?
            _records[i] = NdefRecord();
        }

        _recordCount = rhs._recordCount;
        for (unsigned int i = 0; i < _recordCount; i++)
        {
            _records[i] = rhs._records[i];
        }
    }
    return *this;
}

unsigned int NdefMessage::getRecordCount()
{
    return _recordCount;
}

int NdefMessage::getEncodedSize()
{
    int size = 0;
    for (unsigned int i = 0; i < _recordCount; i++)
    {
        size += _records[i].getEncodedSize();
    }
    return size;
}

// TODO change this to return uint8_t*
void NdefMessage::encode(uint8_t* data)
{
    // assert sizeof(data) >= getEncodedSize()
    uint8_t* data_ptr = &data[0];

    for (unsigned int i = 0; i < _recordCount; i++)
    {
        _records[i].encode(data_ptr, i == 0, (i + 1) == _recordCount);
        // TODO can NdefRecord.encode return the record size?
        data_ptr += _records[i].getEncodedSize();
    }

}

bool NdefMessage::addRecord(NdefRecord& record)
{

    // LE_INFO("_recordCount: %d", _recordCount);
    if (_recordCount < MAX_NDEF_RECORDS)
    {
        _records[_recordCount] = record;
        _recordCount++;
        return true;
    }
    else
    {
        LE_INFO("WARNING: Too many records. Increase MAX_NDEF_RECORDS.");
        return false;
    }
}

void NdefMessage::addMimeMediaRecord(String mimeType, String payload)
{

    byte payloadBytes[payload.length() + 1];
    payload.getBytes(payloadBytes, sizeof(payloadBytes));

    addMimeMediaRecord(mimeType, payloadBytes, payload.length());
}

void NdefMessage::addMimeMediaRecord(String mimeType, uint8_t* payload, int payloadLength)
{
    NdefRecord r = NdefRecord();
    r.setTnf(TNF_MIME_MEDIA);

    byte type[mimeType.length() + 1];
    mimeType.getBytes(type, sizeof(type));
    r.setType(type, mimeType.length());

    r.setPayload(payload, payloadLength);

    addRecord(r);
}

void NdefMessage::addUnknownRecord(byte *payload, int payloadLength)
{
    NdefRecord r = NdefRecord();
    r.setTnf(TNF_UNKNOWN);

    r.setType(payload, 0);
    r.setPayload(payload, payloadLength);
    addRecord(r);
}


void NdefMessage::addTextRecord(String text)
{
    addTextRecord(text, "en");
}

void NdefMessage::addTextRecord(String text, String encoding)
{
    NdefRecord r = NdefRecord();
    r.setTnf(TNF_WELL_KNOWN);

    uint8_t RTD_TEXT[1] = { 0x54 }; // TODO this should be a constant or preprocessor
    r.setType(RTD_TEXT, sizeof(RTD_TEXT));

    // X is a placeholder for encoding length
    // TODO is it more efficient to build w/o string concatenation?
    String payloadString = "X" + encoding + text;

    byte payload[payloadString.length() + 1];
    payloadString.getBytes(payload, sizeof(payload));

    // replace X with the real encoding length
    payload[0] = encoding.length();

    r.setPayload(payload, payloadString.length());

    addRecord(r);
}

void NdefMessage::addUriRecord(String uri)
{
    NdefRecord* r = new NdefRecord();
    r->setTnf(TNF_WELL_KNOWN);

    uint8_t RTD_URI[1] = { 0x55 }; // TODO this should be a constant or preprocessor
    r->setType(RTD_URI, sizeof(RTD_URI));

    // X is a placeholder for identifier code
    String payloadString = "X" + uri;

    byte payload[payloadString.length() + 1];
    payloadString.getBytes(payload, sizeof(payload));

    // add identifier code 0x0, meaning no prefix substitution
    payload[0] = 0x0;

    r->setPayload(payload, payloadString.length());

    addRecord(*r);
    delete(r);
}

void NdefMessage::addEmptyRecord()
{
    NdefRecord* r = new NdefRecord();
    r->setTnf(TNF_EMPTY);
    addRecord(*r);
    delete(r);
}

NdefRecord NdefMessage::getRecord(int index)
{
    if (index > -1 && (unsigned int) index < _recordCount)
    {
        return _records[index];
    }
    else
    {
        return NdefRecord(); // would rather return NULL
    }
}

NdefRecord NdefMessage::operator[](int index)
{
    return getRecord(index);
}

void NdefMessage::print()
{
    LE_INFO("\nNDEF Message recordCount: %d record%s %d bytes",
	_recordCount, (_recordCount == 1) ? ", " : "s, ", getEncodedSize());

    unsigned int i;
    for (i = 0; i < _recordCount; i++)
    {
         _records[i].print();
    }
}
