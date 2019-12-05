//--------------------------------------------------------------------------------------------------
/**
 * @file magn.h
 *
 * Magnetometer (MAGN) sensor interface.  Provides an interface between the Linux
 * kernel driver for the MAGN  and the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef MAGN_H_INCLUDE_GUARD
#define MAGN_H_INCLUDE_GUARD

LE_SHARED le_result_t mangOH_ReadMagn(double *xMagn, double *yMagn, double *zMagn);

#endif // MAGN_H_INCLUDE_GUARD
