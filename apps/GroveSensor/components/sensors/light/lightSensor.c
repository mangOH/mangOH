//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Yellow Light sensor interface.
 *
 * Provides the gas API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "lightSensor.h"

const char lightSensorAdc[] = "EXT_ADC0";

//--------------------------------------------------------------------------------------------------
/**
 * Read Light measurement
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t light_Read(double *light_value)
{
    int32_t valueMv;
    
    le_result_t r = le_adc_ReadValue(lightSensorAdc, &valueMv);

    if (r != LE_OK)
    {
        return r;
    }

    *light_value = valueMv/1000.0;
    
    return LE_OK;
}


static void SampleLight
(
    psensor_Ref_t ref,
    void *context
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
    psensor_Create("light", DHUBIO_DATA_TYPE_NUMERIC, "", SampleLight, NULL);
}


