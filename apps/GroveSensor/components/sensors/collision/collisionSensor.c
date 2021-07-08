//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Air sensor interface.
 *
 * Provides the Collision API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Collision Sensor can detect whether any collision movement or vibration happens
 * It will output a low pulse signal when vibration is detected. 
 * 
 * For more detail refer to: http://wiki.seeedstudio.com/Grove-Collision_Sensor/
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "collisionSensor.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read Collision state
 * Output is low level if collision is detected
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t collision_Read(uint8_t *value)
{
    *value = D5_Read();
    return LE_OK;
}

static void SampleCollision
(
    psensor_Ref_t ref,
    void *context
)
{
    uint8_t sample;

    le_result_t result = collision_Read(&sample);

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
    LE_FATAL_IF(
        D5_SetInput(D5_ACTIVE_HIGH) != LE_OK,
        "Failed to setup Digital Port as default input high");

    psensor_Create("collision", DHUBIO_DATA_TYPE_NUMERIC, "", SampleCollision, NULL);
}
