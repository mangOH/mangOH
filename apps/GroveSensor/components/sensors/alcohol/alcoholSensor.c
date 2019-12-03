//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Alcohol sensor interface.
 *
 * Provides the gas API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "alcoholSensor.h"

const char alcoholSensorAdc[] = "EXT_ADC0";

//--------------------------------------------------------------------------------------------------
/**
 * Read Alcohol measurement
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t alcohol_Read(double *alcoholRs)
{
    int32_t adcValue;
    double sensorVolt;
    double RS; //Sensing Resistance: resistance of the sensor reduce when gas concentration increases 
    
    le_result_t r = le_adc_ReadValue(alcoholSensorAdc, &adcValue);

    if (r != LE_OK)
    {
        return r;
    }

    //Convert adc value to voltage
    sensorVolt = adcValue/1024*5.0;

    //Get Sensing Resistance value 
    //Rs is a dimensionless ratio that is useful for calculating the sensed alcohol concentration
    RS = sensorVolt/(5.0 - sensorVolt);

    *alcoholRs = RS;
    
    return LE_OK;
}


static void SampleAlcohol
(
    psensor_Ref_t ref,
    void *context
)
{
    double sample;

    le_result_t result = alcohol_Read(&sample);

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
    psensor_Create("alcohol", DHUBIO_DATA_TYPE_NUMERIC, "", SampleAlcohol, NULL);
}


