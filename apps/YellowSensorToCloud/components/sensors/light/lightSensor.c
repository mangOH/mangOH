//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red light sensor interface.
 *
 * Provides the accelerometer and gyro IPC API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "lightSensor.h"
#include "fileUtils.h"

static const char LightFile[]  = "/driver/in_illuminance_input";


//--------------------------------------------------------------------------------------------------
/**
 * Read the light  measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t light_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in LUX ) will be put if LE_OK is returned.
)
{
    double light;
    le_result_t r = file_ReadDouble( LightFile, &light);
    if (r != LE_OK)
    {
        return r;
    }
    *readingPtr = ((double)light);

    return LE_OK;
}



static void SampleLight
(
    psensor_Ref_t ref, void *context
)
{
    double sample;

    le_result_t result = light_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read sensor (%s).", LE_RESULT_TXT(result));
    }
}




COMPONENT_INIT
{
    psensor_Create("light", DHUBIO_DATA_TYPE_NUMERIC, "lux", SampleLight, NULL);
}


