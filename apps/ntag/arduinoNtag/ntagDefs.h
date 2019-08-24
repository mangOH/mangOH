/* SWI defines for ntag_main script */
#ifndef NTAGDEFS_H
#define NTAGDEFS_H

#include "ntageepromadapter.h"

#define HARDI2C


void ReadTagExtended(void);
void WriteTagMultipleRecords(void);
void WriteTag(void);
void EraseTag(void);
void CleanTag(void);
void ReadTag(void);

#endif // NTAGDEFS_H
