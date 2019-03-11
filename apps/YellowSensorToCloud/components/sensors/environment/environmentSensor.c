//--------------------------------------------------------------------------------------------------
/**
 * @file pressureSensor.c
 *
 * Implementation of the mangOH Yellow  pressure/temperature/gas/humidity sensor interface component.
 *
 * Publishes the pressure, temperature, humidity and gas  readings to the Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "fileUtils.h"

static const char PressureFile[] = "/driver/in_pressure_input";
static const char TemperatureFile[] = "/driver/in_temp_input";
static const char GasFile[] = "/driver/in_resistance_input";
static const char HumidityFile[] = "/driver/in_humidityrelative_input";

static void SamplePressure
(
    psensor_Ref_t ref
)
{
    double sample;

    le_result_t result = pressure_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read sensor (%s).", LE_RESULT_TXT(result));
    }
}


static void SampleTemperature
(
    psensor_Ref_t ref
)
{
    double sample;

    le_result_t result = temperature_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read sensor (%s).", LE_RESULT_TXT(result));
    }
}

static void SampleGas
(
    psensor_Ref_t ref
)
{
    double sample;
    le_result_t result = gas_Read(&sample);
   
    if (result == LE_OK)
    {
	psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
	LE_ERROR("Failed to read sensor (%s)", LE_RESULT_TXT(result));
    }
}

static void SampleHumidity
(
    psensor_Ref_t ref
)
{
    double sample;
    le_result_t result = humidity_Read(&sample);
   
    if (result == LE_OK)
    {
	psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
	LE_ERROR("Failed to read sensor (%s)", LE_RESULT_TXT(result));
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Read the air pressure measurement in kiloPascals (kPa).
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t pressure_Read
(
    double* readingPtr
        ///< [OUT] Where the pressure reading (kPa) will be put if LE_OK is returned.
)
{
    return file_ReadDouble(PressureFile, readingPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Read the temperature measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t temperature_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in degrees C) will be put if LE_OK is returned.
)
{
    int temp;
    le_result_t r = file_ReadInt(TemperatureFile, &temp);
    if (r != LE_OK)
    {
        return r;
    }

    // The divider is 1000 based on the comments in the kernel driver on bmp280_compensate_temp()
    // which is called by bmp280_read_temp()
    *readingPtr = ((double)temp);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read the gas measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t gas_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in kilo Ohms) will be put if LE_OK is returned.
)
{
    int gas;
    le_result_t r = file_ReadInt(GasFile, &gas);
    if (r != LE_OK)
    {
        return r;
    }

    // The divider is 1000 based on the comments in the kernel driver on bmp280_compensate_gas()
    // which is called by bmp280_read_gas()
    *readingPtr = ((double)gas) / 1000.0;

    return LE_OK;


}

//--------------------------------------------------------------------------------------------------
/**
 * Read the humidity measurement.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------
le_result_t humidity_Read
(
    double* readingPtr
        ///< [OUT] Where the reading (in %rH) will be put if LE_OK is returned.
)
{
    int humidity;
    le_result_t r = file_ReadInt(HumidityFile, &humidity);
    if (r != LE_OK)
    {
        return r;
    }
    *readingPtr = (double)humidity ;

    return LE_OK;

}


COMPONENT_INIT
{
    // Use the periodic sensor component from the Data Hub to implement the timers and the
    // interface to the Data Hub.
    psensor_Create("environment/pressure", DHUBIO_DATA_TYPE_NUMERIC, "kPa", SamplePressure);
    psensor_Create("environment/temp", DHUBIO_DATA_TYPE_NUMERIC, "degC", SampleTemperature);
    psensor_Create("environment/gas", DHUBIO_DATA_TYPE_NUMERIC, "kOhms", SampleGas);
    psensor_Create("environment/humidity", DHUBIO_DATA_TYPE_NUMERIC, "%rH", SampleHumidity);

}
