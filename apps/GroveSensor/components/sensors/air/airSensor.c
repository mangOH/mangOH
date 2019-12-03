//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Red Air sensor interface.
 *
 * Provides the Air API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * The standard particulate matter mass concentration value refers to the mass concentration value
 * obtained by density conversion of industrial metal particles as equivalent particles, and is
 * suitable for use in industrial production workshops and the like.  The concentration of
 * particulate matter in the atmospheric environment is converted by the density of the main
 * pollutants in the air as equivalent particles, and is suitable for ordinary indoor and outdoor
 * atmospheric environments.  So you can see that there are two sets of data above.
 * For more detail refer to: http://wiki.seeedstudio.com/Grove-Laser_PM2.5_Sensor-HM3301/
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "airSensor.h"
#include "i2cUtils.h"

#define AIR_I2C_ADDR        0x40
static const char *air_sensor_i2c_bus = "/dev/i2c-5";
static uint8_t buf[29];

#define HM3301_UART_DISABLE_CMD 0x88

// NOTE: LSB follows the MSB
#define DATA_STANDARD_PARTICULATE_PM1_0_MSB                 5
#define DATA_STANDARD_PARTICULATE_PM2_5_MSB                 7
#define DATA_STANDARD_PARTICULATE_PM10_MSB                  9
#define DATA_ATMOSPHERIC_ENV_PARTICULATE_PM1_0_MSB         11
#define DATA_ATMOSPHERIC_ENV_PARTICULATE_PM2_5_MSB         13
#define DATA_ATMOSPHERIC_ENV_PARTICULATE_PM10_MSB          15

enum HM3301MeasurementType
{
    MEAS_T_STANDARD_PARICULATE_PM1_0,
    MEAS_T_STANDARD_PARICULATE_PM2_5,
    MEAS_T_STANDARD_PARICULATE_PM10,
    MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM1_0,
    MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM2_5,
    MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM10,
};

static const struct HM3301Measurement
{
    const char *name;
    const char *dhubResource;
    size_t msbOffset;
} Measurements[] = {
    [MEAS_T_STANDARD_PARICULATE_PM1_0] = {
        "Standard Particulate PM1.0",
        "standardParticulatePM1_0",
        DATA_STANDARD_PARTICULATE_PM1_0_MSB,
    },
    [MEAS_T_STANDARD_PARICULATE_PM2_5] = {
        "Standard Particulate PM2.5",
        "standardParticulatePM2_5",
        DATA_STANDARD_PARTICULATE_PM2_5_MSB,
    },
    [MEAS_T_STANDARD_PARICULATE_PM10] = {
        "Standard Particulate PM10",
        "standardParticulatePM10",
        DATA_STANDARD_PARTICULATE_PM10_MSB,
    },
    [MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM1_0] = {
        "Atmospheric Environment Particulate PM1.0",
        "atmosphericParticulatePM1_0",
        DATA_STANDARD_PARTICULATE_PM1_0_MSB,
    },
    [MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM2_5] = {
        "Atmospheric Environment Particulate PM2.5",
        "atmosphericParticulatePM2_5",
        DATA_STANDARD_PARTICULATE_PM2_5_MSB,
    },
    [MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM10] = {
        "Atmospheric Environment Particulate PM10",
        "atmosphericParticulatePM10",
        DATA_STANDARD_PARTICULATE_PM10_MSB,
    },
};


//--------------------------------------------------------------------------------------------------
/**
 * Read all of the registers from the device into the buf variable
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ReadAllHM3301Data
(
    void
)
{
    LE_INFO("Start Reading Sensor");
    int res = i2cReceiveBytes(air_sensor_i2c_bus, AIR_I2C_ADDR, buf, sizeof(buf));
    if (res != 0)
    {
        return LE_FAULT;
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read a single measurement from the HM3301
 */
//--------------------------------------------------------------------------------------------------
static le_result_t ReadHM3301Generic
(
    const struct HM3301Measurement *measurement, ///< specifier of measurement to read
    uint16_t *sample  ///< the value that is read
)
{
    le_result_t res = ReadAllHM3301Data();
    if (res != LE_OK)
    {
        return res;
    }

    *sample = (buf[measurement->msbOffset] << 8) | (buf[measurement->msbOffset + 1] << 0);
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read a single measurement from the HM3301 and push the measurement into the data hub
 */
//--------------------------------------------------------------------------------------------------
static void SampleHM3301Generic
(
    psensor_Ref_t ref, ///< psensor instance reference
    void *context  ///< expected to be set to the measurement to be sampled
)
{
    const struct HM3301Measurement *measurement = context;
    uint16_t sample;
    le_result_t res = ReadHM3301Generic(measurement, &sample);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to read %s value (%s).", measurement->name, LE_RESULT_TXT(res));
        return;
    }

    psensor_PushNumeric(ref, 0, sample);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Industrial Condition
 *
 * PM1.0 are particles less than 1 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadStandardPM1_0
(
    uint16_t *value  ///< ug/m3 of PM1.0 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_STANDARD_PARICULATE_PM1_0], value);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Industrial Condition
 *
 * PM2.5 are particles less than 2.5 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadStandardPM2_5
(
    uint16_t *value  ///< ug/m3 of PM2.5 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_STANDARD_PARICULATE_PM2_5], value);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Industrial Condition
 *
 * PM1.0 are particles less than 10 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadStandardPM10
(
    uint16_t *value  ///< ug/m3 of PM10 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_STANDARD_PARICULATE_PM10], value);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Environment Condition
 *
 * PM1.0 are particles less than 1 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadAtmosphericEnvironmentPM1_0
(
    uint16_t *value  ///< ug/m3 of PM1.0 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM1_0], value);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Environment Condition
 *
 * PM2.5 are particles less than 2.5 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadAtmosphericEnvironmentPM2_5
(
    uint16_t *value  ///< ug/m3 of PM2.5 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM2_5], value);
}

//--------------------------------------------------------------------------------------------------
/**
 * Read Air measurement in Environment Condition
 *
 * PM10 are particles less than 10 µm in diameter.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t air_ReadAtmosphericEnvironmentPM10
(
    uint16_t *value  ///< ug/m3 of PM2.5 particulate
)
{
    return ReadHM3301Generic(&Measurements[MEAS_T_ATMOSPHERIC_ENV_PARICULATE_PM10], value);
}


COMPONENT_INIT
{
    LE_FATAL_IF(
       i2cSendByte(air_sensor_i2c_bus, AIR_I2C_ADDR, HM3301_UART_DISABLE_CMD) != 0,
       "Failed to disable UART communication");
    for (int i = 0; i < NUM_ARRAY_MEMBERS(Measurements); i++)
    {
        psensor_Create(
            Measurements[i].dhubResource,
            DHUBIO_DATA_TYPE_NUMERIC,
            "ug/m3",
            SampleHM3301Generic,
            (void *)&Measurements[i]);
    }
}
