/**
 * @file
 *
 * This app waits for updates to the dhub rtc time and then writes the temp/time to eink.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"

#define PERIOD			60

// Data Hub resource paths.
#define RES_PATH_RTC_ENABLE	"/app/rtcDhubIf/rtc/enable"
#define RES_PATH_RTC_PERIOD	"/app/rtcDhubIf/rtc/period"
#define RES_PATH_RTC_VALUE	"/app/rtcDhubIf/rtc/value"
#define RES_PATH_TEMP_PERIOD	"/app/imu/temp/period"
#define RES_PATH_TEMP_ENABLE	"/app/imu/temp/enable"
#define RES_PATH_TEMP_VALUE	"/app/imu/temp/value"
#define RES_PATH_WRITE_EINK	"/app/einkDhubIf/value"


//--------------------------------------------------------------------------------------------------
/**
 * RTC update handler that sends the time & temp to to Eink
 */
//--------------------------------------------------------------------------------------------------
static void RtcUpdateHandler
(
    double timestamp,
    const char *timep,
    void* contextPtr
)
{
   double ts;          // throwaway
   double Temperature;
   char EinkWriteBuffer[BUFSIZ];

   le_result_t result = query_GetNumeric(RES_PATH_TEMP_VALUE, &ts, &Temperature);
   if (result == LE_OK)
   {
       sprintf(EinkWriteBuffer, "Time:\n%s\nTemp:\n%.5f", timep, Temperature);
       LE_DEBUG("Writing: %s to Eink", EinkWriteBuffer);
       admin_PushString(RES_PATH_WRITE_EINK, 0, EinkWriteBuffer);
   }
}


COMPONENT_INIT
{
    admin_SetNumericDefault(RES_PATH_RTC_PERIOD, PERIOD);
    admin_SetBooleanDefault(RES_PATH_RTC_ENABLE, true);
    admin_SetNumericDefault(RES_PATH_TEMP_PERIOD, PERIOD);
    admin_SetBooleanDefault(RES_PATH_TEMP_ENABLE, true);
    admin_AddStringPushHandler(RES_PATH_RTC_VALUE, RtcUpdateHandler, NULL);
}
