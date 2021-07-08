//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Yellow Color sensor interface.
 *
 * Provides the Color API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
 * Based on the 8*2 array of filtered photodiodes and 16-bit analog-to-digital converters, you can
 * measure the color chromaticity of ambient light or the color of objects. 
 * For more detail refer to: http://wiki.seeedstudio.com/Grove-I2C_Color_Sensor/
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "colorSensor.h"
#include "i2cUtils.h"

#define COLOR_I2C_ADDR 0x29
static const char *color_sensor_i2c_bus = "/dev/i2c-5";

#define TCS34725_ID 0x12

#define TCS34725_INTEGRATIONTIME_2_4MS 0xFF
#define TCS34725_INTEGRATIONTIME_24MS 0xF6
#define TCS34725_INTEGRATIONTIME_50MS 0xEB
#define TCS34725_INTEGRATIONTIME_101MS 0xD5
#define TCS34725_INTEGRATIONTIME_154MS 0xC0
#define TCS34725_INTEGRATIONTIME_700MS 0x00

#define TCS34725_GAIN_1X 0x00
#define TCS34725_GAIN_4X 0x01
#define TCS34725_GAIN_16X 0x02
#define TCS34725_GAIN_60X 0x03

#define TCS34725_CDATAL 0x14
#define TCS34725_CDATAH 0x15
#define TCS34725_RDATAL 0x16
#define TCS34725_RDATAH 0x17
#define TCS34725_GDATAL 0x18
#define TCS34725_GDATAH 0x19
#define TCS34725_BDATAL 0x1A
#define TCS34725_BDATAH 0x1B

#define TCS34725_ENABLE 0x00
#define TCS34725_ENABLE_AIEN 0x10
#define TCS34725_ENABLE_WEN 0x08
#define TCS34725_ENABLE_AEN 0x02
#define TCS34725_ENABLE_PON 0x01

//--------------------------------------------------------------------------------------------------
/**
 * Measure and convert color of sensor to hex code
 */
//--------------------------------------------------------------------------------------------------
le_result_t color_Read(
    char *color_hex,
    ///< [OUT]
    size_t color_hexSize
    ///< [IN]
)
{ 
    LE_INFO("Start Reading Sensor");
    uint8_t data[2];
    int res = i2cReceiveBytes_v2(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_CDATAL|0x80, data, sizeof(data));
    if (res != 0)
    {
        return LE_FAULT;
    }
	uint16_t clear	= data[1] * 256 + data[0];

    res = i2cReceiveBytes_v2(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_RDATAL|0x80, data, sizeof(data));
    if (res != 0)
    {
        return LE_FAULT;
    }
    uint16_t red	= data[1] * 256 + data[0];

    res = i2cReceiveBytes_v2(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_GDATAL|0x80, data, sizeof(data));
    if (res != 0)
    {
        return LE_FAULT;
    }
    uint16_t green	= data[1] * 256 + data[0];

    res = i2cReceiveBytes_v2(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_BDATAL|0x80, data, sizeof(data));
    if (res != 0)
    {
        return LE_FAULT;
    }
    uint16_t blue	= data[1] * 256 + data[0];

    // Figure out some basic hex code for visualization
    float r, g, b;
    r = red; r /= clear;
    g = green; g /= clear;
    b = blue; b /= clear;
    r *= 256; g *= 256; b *= 256;

    snprintf(color_hex, color_hexSize, "%x%x%x", (int)r, (int)g, (int)b);

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Read a color and push the measurement into the data hub
 */
//--------------------------------------------------------------------------------------------------
static void SampleColor(
    psensor_Ref_t ref, ///< psensor instance reference
    void *context      ///< expected to be set to the measurement to be sampled
)
{
    char sample[6];

    le_result_t result = color_Read(sample, 7);

    if (result == LE_OK)
    {
        psensor_PushString(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read sensor (%s).", LE_RESULT_TXT(result));
    }
}

COMPONENT_INIT
{
    //Read data
    uint8_t data[1];
    LE_FATAL_IF(
        i2cReceiveBytes_v2(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_ID | 0x80, data, 1) != 0,
        "Failed to read Sensor ID");
    if(data[0] != 0x44)
    {
        LE_ERROR("Could not get sensor TCS34725 ID");
    }
    
    //set integration time
    LE_FATAL_IF(
        i2cSendByte(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_INTEGRATIONTIME_50MS | 0x80) != 0,
        "Failed to set Integraion time");

    //set gain
    LE_FATAL_IF(
        i2cSendByte(color_sensor_i2c_bus, COLOR_I2C_ADDR, TCS34725_GAIN_4X | 0x80) != 0,
        "Failed to set gain");

    uint8_t ENABLE[]= {TCS34725_ENABLE |0x80 , TCS34725_ENABLE_PON};
    uint8_t ENABLE_PON[]= {TCS34725_ENABLE|0x80,  TCS34725_ENABLE_PON | TCS34725_ENABLE_AEN};

    LE_FATAL_IF(
        i2cSendBytes(color_sensor_i2c_bus, COLOR_I2C_ADDR, ENABLE,2) != 0,
        "Failed to enable color sensor");
    usleep(3000);

    LE_FATAL_IF(
        i2cSendBytes(color_sensor_i2c_bus, COLOR_I2C_ADDR, ENABLE_PON, 2) != 0,
        "Failed to enable color sensor");

    psensor_Create("color", DHUBIO_DATA_TYPE_STRING, "", SampleColor, NULL);
}
