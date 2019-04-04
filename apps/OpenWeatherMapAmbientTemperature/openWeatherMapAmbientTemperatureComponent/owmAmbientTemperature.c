#include "legato.h"
#include "interfaces.h"

#include "openWeatherMap.h"

/*
 * TODO:
 * We should be bound to a positioning service or provide a way for clients to set the position.
 */
static struct
{
    double latitude;
    double longitude;
} Location = {49.172781, -123.071147};
static char ApiKey[32 + 1];

static struct
{
    // Must hold this lock to read/write the other values
    le_mutex_Ref_t lock;
    int64_t timestampNs;
    double temperature;
} LastResult;
static le_data_RequestObjRef_t DataRequest = NULL;
static le_thread_Ref_t HttpThread = NULL;
static le_timer_Ref_t UpdateTimer;

static int64_t GetTimestampNs(void)
{
    struct timespec monotime;
    LE_ASSERT(clock_gettime(CLOCK_MONOTONIC, &monotime) == 0);
    return (monotime.tv_sec * 1000LL * 1000LL * 1000LL) + (monotime.tv_nsec);
}

le_result_t mangOH_ambientTemperature_Read(double *temperature)
{
    le_mutex_Lock(LastResult.lock);
    *temperature = LastResult.temperature;
    le_mutex_Unlock(LastResult.lock);
    return LE_OK;
}

static le_result_t UpdateAmbientTemperature(void)
{
    json_t *response = OpenWeatherMapGet(Location.latitude, Location.longitude, ApiKey);
    if (!response)
    {
        return LE_FAULT;
    }

    double temperature;
    const int unpackRes = json_unpack(response, "{s:{s:f}}", "main", "temp", &temperature);
    json_decref(response);
    if (unpackRes != 0)
    {
        return LE_FAULT;
    }

    LE_DEBUG("Received OpenWeatherMap temperature: %f", temperature);

    le_mutex_Lock(LastResult.lock);
    LastResult.timestampNs = GetTimestampNs();
    LastResult.temperature = temperature;
    le_mutex_Unlock(LastResult.lock);

    return LE_OK;
}

void UpdateTimerHandler(le_timer_Ref_t timer)
{
    UpdateAmbientTemperature();
}

void *HttpThreadFunc(void *context)
{
    while (UpdateAmbientTemperature() != LE_OK)
    {
        sleep(20);
    }

    // A temperature is available, so advertise the service
    mangOH_ambientTemperature_AdvertiseService();

    UpdateTimer = le_timer_Create("owm ambient");
    LE_ASSERT_OK(le_timer_SetHandler(UpdateTimer, UpdateTimerHandler));
    LE_ASSERT_OK(le_timer_SetMsInterval(UpdateTimer, 1000 * 60 * 10));
    LE_ASSERT_OK(le_timer_SetRepeat(UpdateTimer, 0));
    LE_ASSERT_OK(le_timer_Start(UpdateTimer));

    le_event_RunLoop();
    return NULL;
}

static void DcsStateHandler
(
    const char *intfName,
    bool isConnected,
    void *context
)
{
    if (isConnected)
    {
        LE_INFO("Data connection established using interface %s", intfName);
        // Now that we have a connection, start the thread which updates the temperature
        if (!HttpThread)
        {
            HttpThread = le_thread_Create("owm http", HttpThreadFunc, NULL);
            le_thread_Start(HttpThread);
        }
    }
    else
    {
        LE_WARN("Data connection lost on interface %s", intfName);
    }
}

COMPONENT_INIT
{
    const char *defaultOpenWeatherMapKey = "";
    LE_ASSERT_OK(le_cfg_QuickGetString(
                     "/ApiKey", ApiKey, sizeof(ApiKey), defaultOpenWeatherMapKey));
    LE_FATAL_IF(ApiKey[0] == '\0', "No valid API key was provided");
    LE_INFO("Using OpenWeatherMap key: \"%s\"", ApiKey);

    LastResult.lock = le_mutex_CreateNonRecursive("owm LastResult");

    le_data_AddConnectionStateHandler(DcsStateHandler, NULL);
    LE_DEBUG("Requesting data connection");
    DataRequest = le_data_Request();
}
