//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red light sensor interface.
 *
 * Provides the light sensor IPC API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

// TODO: Remove this when mk tools bug with aliasing [types-only] APIs is fixed.
#define DHUBIO_DATA_TYPE_NUMERIC IO_DATA_TYPE_NUMERIC
#define dhubIO_DataType_t io_DataType_t

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "lightSensor.h"
#include "fileUtils.h"

static const char LightFile[]  = "/driver/in_intensity_input";


//--------------------------------------------------------------------------------------------------
/**
 * Read the light level measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t light_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in LUX) will be put if LE_OK is returned.
)
{
    double light;
    le_result_t r = file_ReadDouble(LightFile, &light);
    if (r != LE_OK)
    {
        return r;
    }
    *readingPtr = ((double)light);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback from the psensor component requesting that a sample be taken from the sensor and pushed
 * to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
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
    (void)psensor_Create("", DHUBIO_DATA_TYPE_NUMERIC, "nW/cm2", SampleLight, NULL);
}
