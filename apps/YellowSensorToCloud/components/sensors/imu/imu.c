//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Inertial Measurement Unit (IMU) sensor interface.
 *
 * Provides the accelerometer and gyro IPC API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "imu.h"
#include "fileUtils.h"
#include "periodicSensor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read the accelerometer's linear acceleration measurement in meters per second squared.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t imu_ReadAccel
(
    double* xPtr,
        ///< [OUT] Where the x-axis acceleration (m/s2) will be put if LE_OK is returned.
    double* yPtr,
        ///< [OUT] Where the y-axis acceleration (m/s2) will be put if LE_OK is returned.
    double* zPtr
        ///< [OUT] Where the z-axis acceleration (m/s2) will be put if LE_OK is returned.
)
{
    le_result_t r;

    double scaling = 0.0;
    r = file_ReadDouble("/driver/in_accel_scale", &scaling);
    if (r != LE_OK)
    {
        goto done;
    }

    r = file_ReadDouble("/driver/in_accel_x_raw", xPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *xPtr *= scaling;

    r = file_ReadDouble("/driver/in_accel_y_raw", yPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *yPtr *= scaling;

    r = file_ReadDouble("/driver/in_accel_z_raw", zPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *zPtr *= scaling;

done:
    return r;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read the gyroscope's angular velocity measurement in radians per seconds.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t imu_ReadGyro
(
    double* xPtr,
        ///< [OUT] Where the x-axis rotation (rads/s) will be put if LE_OK is returned.
    double* yPtr,
        ///< [OUT] Where the y-axis rotation (rads/s) will be put if LE_OK is returned.
    double* zPtr
        ///< [OUT] Where the z-axis rotation (rads/s) will be put if LE_OK is returned.
)
{
    le_result_t r;

    double scaling = 0.0;
    r = file_ReadDouble("/driver/in_anglvel_scale", &scaling);
    if (r != LE_OK)
    {
        goto done;
    }

    r = file_ReadDouble("/driver/in_anglvel_x_raw", xPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *xPtr *= scaling;

    r = file_ReadDouble("/driver/in_anglvel_y_raw", yPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *yPtr *= scaling;

    r = file_ReadDouble("/driver/in_anglvel_z_raw", zPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *zPtr *= scaling;

done:
    return r;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read the temperature measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t temperature_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in degrees C) will be put if LE_OK is returned.
)
{
    le_result_t r;

    double scaling = 0.0;
    r = file_ReadDouble("/driver/in_temp_scale", &scaling);
    if (r != LE_OK)
    {
        LE_ERROR("Failed to read scale");
        goto done;
    }

    double offset = 0.0;
    r = file_ReadDouble("/driver/in_temp_offset", &offset);
    if (r != LE_OK)
    {
        LE_ERROR("Failed to read offset (%s)", LE_RESULT_TXT(r));
        goto done;
    }

    r = file_ReadDouble("/driver/in_temp_raw", readingPtr);
    if (r != LE_OK)
    {
        LE_ERROR("Failed to read raw value (%s)", LE_RESULT_TXT(r));
        goto done;
    }

    *readingPtr = (*readingPtr + offset) * scaling / 1000;

done:
    return r;
}


//--------------------------------------------------------------------------------------------------
/**
 * Sample the gyroscope and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleGyro
(
    psensor_Ref_t ref, void *context
)
{
    double x;
    double y;
    double z;

    le_result_t result = imu_ReadGyro(&x, &y, &z);

    if (result == LE_OK)
    {
        char sample[256];

        int len = snprintf(sample, sizeof(sample), "{\"x\":%lf, \"y\":%lf, \"z\":%lf}", x, y, z);
        if (len >= sizeof(sample))
        {
            LE_FATAL("JSON string (len %d) is longer than buffer (size %zu).", len, sizeof(sample));
        }

        psensor_PushJson(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read gyro (%s).", LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sample the accelerometer and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleAccel
(
    psensor_Ref_t ref, void *context
)
{
    double x;
    double y;
    double z;

    le_result_t result = imu_ReadAccel(&x, &y, &z);

    if (result == LE_OK)
    {
        char sample[256];

        int len = snprintf(sample, sizeof(sample), "{\"x\":%lf, \"y\":%lf, \"z\":%lf}", x, y, z);
        if (len >= sizeof(sample))
        {
            LE_FATAL("JSON string (len %d) is longer than buffer (size %zu).", len, sizeof(sample));
        }

        psensor_PushJson(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read accelerometer (%s).", LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Sample the temperature and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleTemp
(
    psensor_Ref_t ref, void *context
)
{
    double sample;

    le_result_t result = temperature_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read IMU temperature (%s).", LE_RESULT_TXT(result));
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Initializes the IMU component.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    // Use the Periodic Sensor component from the Data Hub to implement the sensor interfaces.
    psensor_Create("gyro", DHUBIO_DATA_TYPE_JSON, "", SampleGyro, NULL);
    psensor_Create("accel", DHUBIO_DATA_TYPE_JSON, "", SampleAccel, NULL);
    psensor_Create("imu/temp", DHUBIO_DATA_TYPE_NUMERIC, "degC", SampleTemp, NULL);
}
