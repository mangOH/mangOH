//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red light sensor interface.
 *
 * Provides the light sensor IPC API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

// TODO: Remove this when mk tools bug with aliasing [types-only] APIs is fixed.
#define DHUBIO_DATA_TYPE_NUMERIC IO_DATA_TYPE_NUMERIC
#define dhubIO_DataType_t io_DataType_t

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "lightSensor.h"
#include "iio.h"


static psensor_Ref_t PeriodicSensorRef;
static struct iio_context *LocalIioContext;
struct iio_channel *IntensityChannel;


//--------------------------------------------------------------------------------------------------
/**
 * Retrieve a light level sample from the sensor.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t light_Read
(
    double* reading  ///< [OUT] Where the reading (in W/m2) will be put if LE_OK is returned.
)
{
    double light;
    if (iio_channel_attr_read_double(IntensityChannel, "input", &light) != 0) {
        LE_ERROR("Error when reading light sensor: %m");
        return LE_FAULT;
    }

    const double cmSquaredPerMeterSquared = 100.0 * 100.0;
    const double nanoWattsPerWatt = 1000000000.0;
    *reading = light * (cmSquaredPerMeterSquared / nanoWattsPerWatt);

    return LE_OK;
}


//--------------------------------------------------------------------------------------------------
/**
 * Callback from the psensor component requesting that a sample be taken from the sensor and pushed
 * to the Data Hub.
 */
//--------------------------------------------------------------------------------------------------
static void SampleLight
(
    psensor_Ref_t ref,
    void *context
)
{
    double sample;
    le_result_t result = light_Read(&sample);
    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
}


COMPONENT_INIT
{
    const char *lightSensorName = "opt3002";
    LocalIioContext = iio_create_local_context();
    LE_ASSERT(LocalIioContext);

    struct iio_device *lightSensor = iio_context_find_device(LocalIioContext, lightSensorName);
    LE_FATAL_IF(lightSensor == NULL, "Couldn't find IIO device named %s", lightSensorName);

    const bool isOutput = false;
    IntensityChannel = iio_device_find_channel(lightSensor, "intensity", isOutput);
    LE_FATAL_IF(IntensityChannel == NULL, "Couldn't find intensity channel");

    LE_FATAL_IF(
        iio_channel_find_attr(IntensityChannel, "input") == NULL,
        "No input attribute on intensity channel");

    PeriodicSensorRef = psensor_Create("", DHUBIO_DATA_TYPE_NUMERIC, "W/m2", SampleLight, NULL);
}
