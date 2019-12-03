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


le_result_t settingPIRMotionPin(void){
    
    le_gpioPin8_SetInput(LE_GPIOPIN8_ACTIVE_HIGH);
    le_gpioPin8_EnablePullUp();

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read PIR MOTION
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t pirMotion_Read(uint8_t *pirValue)
{
    uint8_t state = le_gpioPin8_Read();

    if(state){
        LE_INFO("Detected");
    }
    else{
        LE_INFO("Watching");
    }

    *pirValue = state;
    
    return LE_OK;
}


static void SamplePIRMotion
(
    psensor_Ref_t ref,
    void *context
)
{
    uint8_t sample;

    le_result_t result = pirMotion_Read(&sample);

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
    settingPIRMotionPin();
    psensor_Create("pirMotion", DHUBIO_DATA_TYPE_NUMERIC, "", SamplePIRMotion, NULL);
}


