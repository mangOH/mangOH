//--------------------------------------------------------------------------------------------------
/**
 * @file imu.h
 *
 * Inertial Measurement Unit (IMU) sensor interface.  Provides an interface between the Linux
 * kernel driver for the IMU and the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef IMU_H_INCLUDE_GUARD
#define IMU_H_INCLUDE_GUARD

LE_SHARED le_result_t mangOH_ReadAccelerometer(double *xAcc, double *yAcc, double *zAcc);
LE_SHARED le_result_t mangOH_ReadGyro(double *x, double *y, double *z);
LE_SHARED le_result_t mangOH_ReadImuTemp(double *temperature);

#endif // IMU_H_INCLUDE_GUARD
