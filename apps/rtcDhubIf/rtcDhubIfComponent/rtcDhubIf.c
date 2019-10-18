/**
 * @file
 *
 * This app writes the current system time @ period to dhub.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"

// Data Hub resource paths.
#define RES_PATH_RTC        "rtc"
#define RTC_PERIOD          "rtc/period"
#define DEFAULT_PERIOD      10
#define DATE_LENGTH         128


//--------------------------------------------------------------------------------------------------
/**
 * Sample the clock and publish the results to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleClock
(
    psensor_Ref_t ref, void *context
)
{
    char dateTimeBuf[DATE_LENGTH];

    if (LE_OK != le_clk_GetUTCDateTimeString(LE_CLK_STRING_FORMAT_DATE_TIME, dateTimeBuf, sizeof(dateTimeBuf), NULL))
    {
        LE_ERROR("Unable to retrieve current UTC date/time");
        return;
    }

    psensor_PushString(ref, 0, dateTimeBuf);
}


COMPONENT_INIT
{

    psensor_Create("rtc", DHUBIO_DATA_TYPE_STRING, "", SampleClock, NULL);
    dhubIO_SetNumericDefault(RTC_PERIOD, DEFAULT_PERIOD);

}
