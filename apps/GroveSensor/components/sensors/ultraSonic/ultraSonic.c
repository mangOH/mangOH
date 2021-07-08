/**
 * @file gpioPin.c
 *
 * This is sample Legato CF3 GPIO app by using le_gpio.api.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc.
 */

/* Legato Framework */
#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
//--------------------------------------------------------------------------------------------------
/**
 * Because range of sensor is 3 meters, speed of sonic wave is 343.2 Mps
 * So timeout = 3.5 / 343.2 = 0.010198 second = 10198 micro second
 */
//--------------------------------------------------------------------------------------------------
static uint64_t timeout = 12000L;

//--------------------------------------------------------------------------------------------------
/**
 * Convenience function to get current time as uint64_t.
 *
 * @return
 *      Current time as a uint64_t
 */
//--------------------------------------------------------------------------------------------------
static uint64_t GetCurrentTimestamp(void)
{
    struct timeval tv;
    uint64_t utcMilliSec = 0;
    if(gettimeofday(&tv, NULL) == 0){
        utcMilliSec = tv.tv_sec * 1000000 + tv.tv_usec;
    }
    else{
        LE_INFO("Can't get time!");
    }
    return utcMilliSec;
}

static uint64_t MicrosDiff(uint64_t begin, uint64_t end)
{
    return end - begin;
}

static uint64_t pulseIn(uint64_t state)
{
    uint64_t begin = GetCurrentTimestamp();

    // wait for any previous pulse to end
    while (le_gpioPin8_Read()) if (MicrosDiff(begin, GetCurrentTimestamp()) >= timeout) return 0;
    
    // wait for the pulse to start
    while (!le_gpioPin8_Read()) if (MicrosDiff(begin, GetCurrentTimestamp()) >= timeout) return 0;
    uint64_t pulseBegin = GetCurrentTimestamp();
    
    // wait for the pulse to stop
    while (le_gpioPin8_Read()) if (MicrosDiff(begin, GetCurrentTimestamp()) >= timeout) return 0;
    uint64_t pulseEnd = GetCurrentTimestamp();
    
    return MicrosDiff(pulseBegin, pulseEnd);
}


le_result_t ultraSonic_ReadUltraSonicRanger(uint16_t *value)
{
    *value = 0;
    le_gpioPin8_SetPushPullOutput(LE_GPIOPIN8_ACTIVE_HIGH, true);
    le_gpioPin8_EnablePullUp();

    le_gpioPin8_Deactivate();
    usleep(2);
    le_gpioPin8_Activate();
    usleep(10);
    le_gpioPin8_Deactivate();

    le_gpioPin8_SetInput(LE_GPIOPIN8_ACTIVE_HIGH);

    *value = pulseIn(1)/29/2;
    return LE_OK;
}

static void SampleUltraSonicRanger
(
    psensor_Ref_t ref,
    void *context
)
{
    uint16_t sample;

    le_result_t result = ultraSonic_ReadUltraSonicRanger(&sample);

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
    psensor_Create("ultraSonic", DHUBIO_DATA_TYPE_NUMERIC, "cm", SampleUltraSonicRanger, NULL);
}


