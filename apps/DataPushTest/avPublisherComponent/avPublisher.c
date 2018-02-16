#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/*
 * static function declarations
 */
//--------------------------------------------------------------------------------------------------
static void PushCallbackHandler(le_avdata_PushStatus_t status, void* context);
static uint64_t GetCurrentTimestamp(void);
static void SampleTimerHandler(le_timer_Ref_t timer);
static void AvSessionStateHandler (le_avdata_SessionState_t state, void *context);

//--------------------------------------------------------------------------------------------------
/*
 * variable definitions
 */
//--------------------------------------------------------------------------------------------------

// The maximum amount of time to wait for a reading to exceed a threshold before a publish is
// forced.
static const int IntervalBetweenPublish = 900;

static le_timer_Ref_t SampleTimer;
static le_avdata_RequestSessionObjRef_t AvSession;
static le_avdata_RecordRef_t RecordRef;
static le_avdata_SessionStateHandlerRef_t HandlerRef;

//--------------------------------------------------------------------------------------------------
/*
 * static function definitions
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Handles notification of LWM2M push status.
 *
 * This function will warn if there is an error in pushing data, but it does not make any attempt to
 * retry pushing the data.
 */
//--------------------------------------------------------------------------------------------------
static void PushCallbackHandler
(
    le_avdata_PushStatus_t status, ///< Push success/failure status
    void* context                  ///< Not used
)
{
    switch (status)
    {
    case LE_AVDATA_PUSH_SUCCESS:
        // data pushed successfully
        break;

    case LE_AVDATA_PUSH_FAILED:
        LE_WARN("Push was not successful");
        break;

    default:
        LE_ERROR("Unhandled push status %d", status);
        break;
    }
}

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
    gettimeofday(&tv, NULL);
    uint64_t utcMilliSec = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
    return utcMilliSec;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler for the sensor sampling timer
 */
//--------------------------------------------------------------------------------------------------
static void SampleTimerHandler
(
    le_timer_Ref_t timer  ///< Sensor sampling timer
)
{
    char path[64];
    char strValue[64];
    le_result_t result;
    uint64_t now = GetCurrentTimestamp();
    float floatValue;
    int32_t intValue;

    strncpy(path, "HpumpPct", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Mair", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Mpump", sizeof(path));
    strncpy(strValue, "ON", sizeof(strValue));
    result = le_avdata_RecordString(RecordRef, path, strValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordString() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "AtmMode", sizeof(path));
    intValue = 15;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Tpump", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Pmem", sizeof(path));
    intValue = 1020;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "O2reading", sizeof(path));
    floatValue = 13.00;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "CO2reading", sizeof(path));
    floatValue = 11.00;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "O2setpoint", sizeof(path));
    floatValue = 12.00;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "CO2setpoint", sizeof(path));
    floatValue = 10.00;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Timestamp", sizeof(path));
    strncpy(strValue, "2017-12-07T10:34:58", sizeof(strValue));
    result = le_avdata_RecordString(RecordRef, path, strValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordString() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Veco", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Vexp", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Vhg", sizeof(path));
    intValue = 100;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "SupplyTemperature", sizeof(path));
    floatValue = 12.50;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "ReturnTemperature", sizeof(path));
    floatValue = 15.26;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "RelativeHumidity", sizeof(path));
    intValue = 50;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "AmbientTemperature", sizeof(path));
    intValue = 40;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "USDA1", sizeof(path));
    floatValue = 13.40;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "USDA2", sizeof(path));
    floatValue = 14.66;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "USDA3", sizeof(path));
    floatValue = 14.32;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "CargoTemp", sizeof(path));
    floatValue = 15.82;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "TemperatureSetpoint", sizeof(path));
    floatValue = 10.00;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "AirExchange", sizeof(path));
    intValue = 234;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "SuctionPressure", sizeof(path));
    floatValue = 5.2;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "DischargePressure", sizeof(path));
    floatValue = 4.2;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "PowerFrequency", sizeof(path));
    intValue = 60;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "PowerVoltage", sizeof(path));
    intValue = 400;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Current1", sizeof(path));
    floatValue = 25.5;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Current2", sizeof(path));
    floatValue = 25.5;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Current3", sizeof(path));
    floatValue = 25.5;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Ifc", sizeof(path));
    floatValue = 25.5;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "CompressorFrequency", sizeof(path));
    intValue = 200;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "HeaterPct", sizeof(path));
    intValue = 11;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "EvaporatorMotorStatus", sizeof(path));
    intValue = 11;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "CondenserFanMotorStatus", sizeof(path));
    intValue = 11;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Power Voltage", sizeof(path));
    intValue = 80;
    result = le_avdata_RecordInt(RecordRef, path, intValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordInt() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Tsupply1", sizeof(path));
    floatValue = 12.72;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "Tsupply2", sizeof(path));
    floatValue = 13.20;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "EvaporatorTemperature", sizeof(path));
    floatValue = 40.20;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    strncpy(path, "SuctionTemperature", sizeof(path));
    floatValue = 25.34;
    result = le_avdata_RecordFloat(RecordRef, path, floatValue, now);
    if (result != LE_OK)
    {
        LE_ERROR("le_avdata_RecordFloat() failed - %s", LE_RESULT_TXT(result));
    }

    result = le_avdata_PushRecord(RecordRef, PushCallbackHandler, NULL);
    if (result != LE_OK)
    {
        LE_ERROR("Failed to push record - %s", LE_RESULT_TXT(result));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Handle changes in the AirVantage session state
 *
 * When the session is started the timer to sample the sensors is started and when the session is
 * stopped so is the timer.
 */
//--------------------------------------------------------------------------------------------------
static void AvSessionStateHandler
(
    le_avdata_SessionState_t state,
    void *context
)
{
    switch (state)
    {
        case LE_AVDATA_SESSION_STARTED:
        {
            // TODO: checking for LE_BUSY is a temporary workaround for the session state problem
            // described below.
            LE_DEBUG("Session Started");
            le_result_t status = le_timer_Start(SampleTimer);
            if (status == LE_BUSY)
            {
                LE_INFO("Received session started when timer was already running");
            }
            else
            {
                LE_ASSERT_OK(status);
            }
            break;
        }

        case LE_AVDATA_SESSION_STOPPED:
        {
            LE_DEBUG("Session Stopped");
            le_result_t status = le_timer_Stop(SampleTimer);
            if (status != LE_OK)
            {
                LE_DEBUG("Record push timer not running");
            }

            break;
        }

        default:
            LE_ERROR("Unsupported AV session state %d", state);
            break;
    }
}

COMPONENT_INIT
{
    RecordRef = le_avdata_CreateRecord();

    SampleTimer = le_timer_Create("Sensor Read");
    LE_ASSERT_OK(le_timer_SetMsInterval(SampleTimer, IntervalBetweenPublish * 1000));
    LE_ASSERT_OK(le_timer_SetRepeat(SampleTimer, 0));
    LE_ASSERT_OK(le_timer_SetHandler(SampleTimer, SampleTimerHandler));

    HandlerRef = le_avdata_AddSessionStateHandler(AvSessionStateHandler, NULL);
    AvSession = le_avdata_RequestSession();
    LE_FATAL_IF(AvSession == NULL, "Failed to request avdata session");
}
