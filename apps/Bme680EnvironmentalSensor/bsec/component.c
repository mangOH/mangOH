//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Data Hub interface to the environment sensor.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

/// Input resource path
#define RES_PATH_VALUE        "value"

/// Data hub resource path for enable output.
#define RES_PATH_ENABLE       "enable"

/// Data hub resource path for low-power mode control output.
#define RES_PATH_LOW_POWER    "lowPower"

/// Ambient temperature output resource path.
/// Providing an accurate measurement of ambient air temperature from another source improves the
/// accuracy of the environmental sensor's measurements by allowing it to compensate for the
/// difference between the device's internal temperature and the ambient air temperature.
#define RES_PATH_AMBIENT_TEMP "ambientAirTemp"

/// Main configuration output resource path
#define RES_PATH_CONFIG       "config"

/// Sample JSON value to report to the Data Hub for auto-discovery by administrative tools.
#define VALUE_EXAMPLE "{\"timestamp\":87455639412210,\"iaqValue\":25.0,\"iaqAccuracy\":0," \
                      "\"co2EquivalentValue\":500.0,\"co2EquivalentAccuracy\":0,\"breathVocValue\""\
                      ":0.49999994039535522,\"breathVocAccuracy\":0,\"pressure\":101621.0," \
                      "\"temperature\":21.949438095092773,\"humidity\":50.673931121826172}"

/// true if at least one ambient temperature sample has been received.
static bool AmbientTempReceived = false;

/// true if the sensor is enabled.
static bool IsEnabled = false;

/// The sampling rate configuration.
/// Defaults to "low power", which is actually the higher power consuming option.
static mangOH_bme680_SamplingRate_t SamplingRate = MANGOH_BME680_SAMPLING_RATE_LP;

//--------------------------------------------------------------------------------------------------
/**
 * Update the sensor configuration based on the configuration settings received from the
 * Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void UpdateSensor
(
    void
)
{
    mangOH_bme680_SamplingRate_t samplingRate = SamplingRate;

    if ((!IsEnabled) || (!AmbientTempReceived))
    {
        samplingRate = MANGOH_BME680_SAMPLING_RATE_DISABLED;
    }

    LE_ASSERT_OK(mangOH_bme680_Configure(samplingRate,
                                         IsEnabled,    // IAQ
                                         IsEnabled,    // CO2
                                         IsEnabled,    // breath VOC
                                         IsEnabled,    // pressure
                                         IsEnabled,    // temperature
                                         IsEnabled) ); // humidity
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called back by the mangOH_bme680 API to pass a new sensor reading to us.
 */
//--------------------------------------------------------------------------------------------------
static void SensorReadingHandler
(
    const mangOH_bme680_Reading_t *reading,
    void* context
)
{
    char value[512];

    size_t len = snprintf(value, sizeof(value), "{\"timestamp\":%lld", reading->timestamp);

    if ((reading->iaq.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"iaqValue\":%lf,\"iaqAccuracy\":%d",
                        reading->iaq.value,
                        reading->iaq.accuracy);
    }

    if ((reading->co2Equivalent.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"co2EquivalentValue\":%lf,\"co2EquivalentAccuracy\":%d",
                        reading->co2Equivalent.value,
                        reading->co2Equivalent.accuracy);
    }

    if ((reading->breathVoc.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"breathVocValue\":%lf,\"breathVocAccuracy\":%d",
                        reading->breathVoc.value,
                        reading->breathVoc.accuracy);
    }

    if ((reading->pressure.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"pressure\":%lf",
                        reading->pressure.value);
    }

    if ((reading->temperature.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"temperature\":%lf",
                        reading->temperature.value);
    }

    if ((reading->humidity.valid) && (len < sizeof(value)))
    {
        len += snprintf(value + len,
                        sizeof(value) - len,
                        ",\"humidity\":%lf",
                        reading->humidity.value);
    }

    if (len < sizeof(value))
    {
        len += snprintf(value + len, sizeof(value) - len, "}");
    }

    LE_ASSERT(len < sizeof(value));

    io_PushJson(RES_PATH_VALUE, IO_NOW, value);
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called by the Data Hub to deliver to us an update of the ambient temperature.
 */
//--------------------------------------------------------------------------------------------------
static void HandleAmbientTempPush
(
    double timestamp,
    double value,   ///< degrees Celcius
    void* contextPtr
)
{
    mangOH_bme680_SetAmbientTemperature(value);

    if (!AmbientTempReceived)
    {
        AmbientTempReceived = true;

        UpdateSensor();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called by the Data Hub to deliver to us an update to our enable setting.
 */
//--------------------------------------------------------------------------------------------------
static void HandleEnablePush
(
    double timestamp,
    bool value,     ///< true = enable, false = disable
    void* contextPtr
)
{
    if (value != IsEnabled)
    {
        IsEnabled = value;
        UpdateSensor();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called by the Data Hub to deliver to us an update to our lowPower setting.
 */
//--------------------------------------------------------------------------------------------------
static void HandleLowPowerPush
(
    double timestamp,
    bool value,     ///< true = low power mode (ULP), false = not as low but still low power (LP).
    void* contextPtr
)
{
    mangOH_bme680_SamplingRate_t newRate;
    if (value)
    {
        newRate = MANGOH_BME680_SAMPLING_RATE_ULP;
    }
    else
    {
        newRate = MANGOH_BME680_SAMPLING_RATE_LP;
    }

    if (newRate != SamplingRate)
    {
        SamplingRate = newRate;
        UpdateSensor();
    }
}


COMPONENT_INIT
{
    LE_ASSERT_OK(io_CreateInput(RES_PATH_VALUE, IO_DATA_TYPE_JSON, ""));
    io_SetJsonExample(RES_PATH_VALUE, VALUE_EXAMPLE);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_AMBIENT_TEMP, IO_DATA_TYPE_NUMERIC, "degC"));
    io_AddNumericPushHandler(RES_PATH_AMBIENT_TEMP, HandleAmbientTempPush, NULL);

    mangOH_bme680_AddSensorReadingHandler(SensorReadingHandler, NULL);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_ENABLE, HandleEnablePush, NULL);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_LOW_POWER, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_LOW_POWER, HandleLowPowerPush, NULL);
    io_MarkOptional(RES_PATH_LOW_POWER);
    io_SetBooleanDefault(RES_PATH_LOW_POWER, false);
}
