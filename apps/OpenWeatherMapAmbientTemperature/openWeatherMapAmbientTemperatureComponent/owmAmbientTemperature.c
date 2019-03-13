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
    int64_t timestampNs;
    double temperature;
} LastResult;
static le_data_RequestObjRef_t DataRequest = NULL;
static bool AmbientTemperatureServiceAdvertised = false;

static int64_t GetTimestampNs(void)
{
    struct timespec monotime;
    LE_ASSERT(clock_gettime(CLOCK_MONOTONIC, &monotime) == 0);
    return (monotime.tv_sec * 1000LL * 1000LL * 1000LL) + (monotime.tv_nsec);
}

le_result_t mangOH_ambientTemperature_Read(double *temperature)
{
    // Use a cached value if the temperature was requested recently
    const int64_t now = GetTimestampNs();
    const int64_t minDelay = 60LL * 1000LL * 1000LL * 1000LL;
    if (LastResult.timestampNs && ((now - LastResult.timestampNs) < minDelay))
    {
        *temperature = LastResult.temperature;
        return LE_OK;
    }

    json_t *response = OpenWeatherMapGet(Location.latitude, Location.longitude, ApiKey);
    if (!response)
    {
        return LE_FAULT;
    }

    const int unpackRes = json_unpack(response, "{s:{s:f}}", "main", "temp", temperature);
    json_decref(response);
    if (unpackRes != 0)
    {
        return LE_FAULT;
    }

    LE_DEBUG("Received ambient temperature reading: %f", *temperature);

    LastResult.timestampNs = now;
    LastResult.temperature = *temperature;
    return LE_OK;
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
        // Now that we have a connection, advertise the ambient temperature service
        if (!AmbientTemperatureServiceAdvertised)
        {
            AmbientTemperatureServiceAdvertised = true;
            mangOH_ambientTemperature_AdvertiseService();
        }
    }
    else
    {
        LE_WARN("Data connection lost on interface %s", intfName);
    }
}

COMPONENT_INIT
{
    const le_result_t cfgRes = le_cfg_QuickGetString(
        "/ApiKey", ApiKey, sizeof(ApiKey), "");
    LE_FATAL_IF(
        cfgRes != LE_OK || ApiKey[0] == '\0',
        "Failed to read OpenWeatherMap API Key from config tree");

    le_data_AddConnectionStateHandler(DcsStateHandler, NULL);
    LE_DEBUG("Requesting data connection");
    DataRequest = le_data_Request();
}
