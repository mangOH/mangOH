/* SWI defines for ntag_main script */
#ifndef NTAGDEFS_H
#define NTAGDEFS_H

#include "ntageepromadapter.h"

#define HARDI2C


/* SWI mangOH Yellow has the 2K Ntag */
static Ntag ntag(Ntag::NTAG_I2C_2K,2,5);
static NtagEepromAdapter ntagAdapter(&ntag);

void ReadTagExtended(void);
void WriteTagMultipleRecords(void);
void WriteTag(void);
void EraseTag(void);
void CleanTag(void);
void ReadTag(void);

#endif // NTAGDEFS_H
