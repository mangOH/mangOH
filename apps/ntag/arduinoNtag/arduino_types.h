/*
 * Arduino Types needed by arduino-ntag - if further Arduino code needs to be
 * needs to be ported than this should header should be moved to a standard
 * headers directory
 */

#ifndef ARDUINO_TYPES_H
#define ARDUINO_TYPES_H

#include <stdint.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>

#define bit(b) (1UL << (b))
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define HEX 16


#include "WCharacter.h"
#include "WString.h"

typedef uint8_t byte;
typedef uint16_t word;

#ifdef __cplusplus
extern "C" {
#endif


void initialiseEpoch (void);

unsigned int millis (void);

void delay (unsigned int howLong);

void delayMicrosecondsHard (unsigned int howLong);

void delayMicroseconds (unsigned int howLong);

#ifdef __cplusplus
}
#endif

#endif /* ARDUINO_TYPES_H */
