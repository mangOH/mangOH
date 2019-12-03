//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Heart Rate sensor interface.
 *
 * Provides the Heart Rate API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * This module is based on optical technology which measures the variation human blood movement 
 * in the vessel
 * For more detail refer to: http://wiki.seeedstudio.com/Grove-Finger-clip_Heart_Rate_Sensor/
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "heartRateSensor.h"
#include "i2cUtils.h"

#define HEART_RATE_I2C_ADDR 0x50
static const char *heart_rate_sensor_i2c_bus = "/dev/i2c-5";
static uint8_t buf[1];

//--------------------------------------------------------------------------------------------------
/**
 * Read all of the registers from the device into the buf variable
 */
//--------------------------------------------------------------------------------------------------
le_result_t heartRate_Read(
    uint8_t *value ///< times
)
{
    int res = i2cReceiveBytes(heart_rate_sensor_i2c_bus, HEART_RATE_I2C_ADDR, buf, 1);
    if (res != 0)
    {
        return LE_FAULT;
    }
    *value = buf[0];
    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Heart Rate measurement 
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
static void SampleHeartRate(
    psensor_Ref_t ref,
    void *context)
{
    uint8_t sample;

    le_result_t result = heartRate_Read(&sample);

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
    psensor_Create("heart_rate", DHUBIO_DATA_TYPE_NUMERIC, "times", SampleHeartRate, NULL);
}
