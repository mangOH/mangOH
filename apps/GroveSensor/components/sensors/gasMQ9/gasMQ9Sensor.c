//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Gas sensor interface.
 *
 * Provides the gas API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "gasMQ9Sensor.h"

const char gasMQ9SensorAdc[] = "EXT_ADC0";
static psensor_Ref_t PeriodicSensorRef;

//--------------------------------------------------------------------------------------------------
/**
 * Read Gas measurement
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t gasMQ9_Read(double *gas_value)
{
    int32_t valueMv;
    
    le_result_t r = le_adc_ReadValue(gasMQ9SensorAdc, &valueMv);

    if (r != LE_OK)
    {
        return r;
    }

    *gas_value = valueMv/1000.0;
    
    return LE_OK;
}


static void SampleGasMQ9
(
    psensor_Ref_t ref,
    void *context
)
{
    double sample;

    le_result_t result = gasMQ9_Read(&sample);

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
    PeriodicSensorRef = psensor_Create("gasmq9", DHUBIO_DATA_TYPE_NUMERIC, "mV", SampleGasMQ9, NULL);
}


