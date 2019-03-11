//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Yellow  Magnetometer sensor interface.
 *
 * Provides the magnetometer IPC API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "magn.h"
#include "fileUtils.h"
#include "periodicSensor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read the magnetometer  measurement in meters per second squared.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t magn_ReadMagn
(
    double* xPtr,
        ///< [OUT] Where the x-axis magnetometer (m/s2) will be put if LE_OK is returned.
    double* yPtr,
        ///< [OUT] Where the y-axis magnetometer (m/s2) will be put if LE_OK is returned.
    double* zPtr
        ///< [OUT] Where the z-axis magnetometer (m/s2) will be put if LE_OK is returned.
)
{
    le_result_t r;

    double scaling = 0.0;
    r = file_ReadDouble("/driver/in_magn_scale", &scaling);
    if (r != LE_OK)
    {
        goto done;
    }

    r = file_ReadDouble("/driver/in_magn_x_raw", xPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *xPtr *= scaling;

    r = file_ReadDouble("/driver/in_magn_y_raw", yPtr);
    if (r != LE_OK)
    {
        goto done;
    }
    *yPtr *= scaling;

    r = file_ReadDouble("/driver/in_magn_z_raw", zPtr);
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
 * Sample the magnetometer and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleMagn
(
    psensor_Ref_t ref
)
{
    double x;
    double y;
    double z;

    le_result_t result = magn_ReadMagn(&x, &y, &z);

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
        LE_ERROR("Failed to read magnetometer (%s).", LE_RESULT_TXT(result));
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
    psensor_Create("magn", DHUBIO_DATA_TYPE_JSON, "", SampleMagn);
}
