//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the mangOH Yellow Digital Light sensor interface.
 *
 * Provides the TSL2561 Digital Light API services and plugs into the Legato Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
/**
* For more detail refer to: http://wiki.seeedstudio.com/Grove-Digital_Light_Sensor/
*/
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include "tsl2561DigitalLight.h"
#include "i2cUtils.h"

#define TSL2561_I2C_ADDR 0x29
static const char *tls2561_sensor_i2c_bus = "/dev/i2c-5";

uint8_t TSL2561_POWER_UP[] = {0x80, 0x03};
uint8_t TSL2561_POWER_DOWN[] = {0x80, 0x00};
uint8_t TSL2561_TIMING[] = {0x81, 0x00};
uint8_t TSL2561_INTERUPT[] = {0x81, 0x00};

#define TSL2561_CHANNEL0L 0x8C
#define TSL2561_CHANNEL0H 0x8D
#define TSL2561_CHANNEL1L 0x8E
#define TSL2561_CHANNEL1H 0x8F

#define LUX_SCALE 14         // scale by 2^14
#define RATIO_SCALE 9        // scale ratio by 2^9
#define CH_SCALE 10          // scale channel values by 2^10
#define CHSCALE_TINT0 0x7517 // 322/11 * 2^CH_SCALE
#define CHSCALE_TINT1 0x0fe7 // 322/81 * 2^CH_SCALE

#define K1T 0x0040 // 0.125 * 2^RATIO_SCALE
#define B1T 0x01f2 // 0.0304 * 2^LUX_SCALE
#define M1T 0x01be // 0.0272 * 2^LUX_SCALE
#define K2T 0x0080 // 0.250 * 2^RATIO_SCA
#define B2T 0x0214 // 0.0325 * 2^LUX_SCALE
#define M2T 0x02d1 // 0.0440 * 2^LUX_SCALE
#define K3T 0x00c0 // 0.375 * 2^RATIO_SCALE
#define B3T 0x023f // 0.0351 * 2^LUX_SCALE
#define M3T 0x037b // 0.0544 * 2^LUX_SCALE
#define K4T 0x0100 // 0.50 * 2^RATIO_SCALE
#define B4T 0x0270 // 0.0381 * 2^LUX_SCALE
#define M4T 0x03fe // 0.0624 * 2^LUX_SCALE
#define K5T 0x0138 // 0.61 * 2^RATIO_SCALE
#define B5T 0x016f // 0.0224 * 2^LUX_SCALE
#define M5T 0x01fc // 0.0310 * 2^LUX_SCALE
#define K6T 0x019a // 0.80 * 2^RATIO_SCALE
#define B6T 0x00d2 // 0.0128 * 2^LUX_SCALE
#define M6T 0x00fb // 0.0153 * 2^LUX_SCALE
#define K7T 0x029a // 1.3 * 2^RATIO_SCALE
#define B7T 0x0018 // 0.00146 * 2^LUX_SCALE
#define M7T 0x0012 // 0.00112 * 2^LUX_SCALE
#define K8T 0x029a // 1.3 * 2^RATIO_SCALE
#define B8T 0x0000 // 0.000 * 2^LUX_SCALE
#define M8T 0x0000 // 0.000 * 2^LUX_SCALE

//--------------------------------------------------------------------------------------------------
/**
 * Retrieve a light level sample from the sensor.
 *
 * @return LE_OK if successful.
 */
//--------------------------------------------------------------------------------------------------

le_result_t tsl2561DigitalLight_Read(
    uint16_t *value ///< lux
)
{
    uint32_t chScale;
    uint32_t channel1;
    uint32_t channel0;
    uint32_t ratio1;
    unsigned int b;
    unsigned int m;
    uint32_t temp;

    uint8_t CH0_LOW, CH0_HIGH, CH1_LOW, CH1_HIGH;
    uint16_t ch0, ch1;

    //Power Up Sensor
    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_POWER_UP, 2);
    usleep(14000);

    i2cReceiveBytes_v2(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_CHANNEL0L, &CH0_LOW, 1);
    i2cReceiveBytes_v2(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_CHANNEL0H, &CH0_HIGH, 1);
    i2cReceiveBytes_v2(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_CHANNEL1L, &CH1_LOW, 1);
    i2cReceiveBytes_v2(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_CHANNEL1H, &CH1_HIGH, 1);

    ch0 = (CH0_HIGH << 8) | CH0_LOW;
    ch1 = (CH1_HIGH << 8) | CH1_LOW;

    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_POWER_DOWN, 2);

    if (ch0 / ch1 < 2 && ch0 > 4900)
    {
        return LE_FAULT; //ch0 out of range, but ch1 not. the lux is not valid in this situation.
    }
    //Calulate lux T package, no gain, 13ms
    chScale = CHSCALE_TINT0;
    chScale = chScale << 4; // scale 1X to 16X

    channel0 = (ch0 * chScale) >> CH_SCALE;
    channel1 = (ch1 * chScale) >> CH_SCALE;

    ratio1 = 0;
    if (channel0 != 0)
        ratio1 = (channel1 << (RATIO_SCALE + 1)) / channel0;
    // round the ratio value
    unsigned long ratio = (ratio1 + 1) >> 1;

    if ((ratio >= 0) && (ratio <= K1T)){
        b = B1T;
        m = M1T;
    }else if (ratio <= K2T){
        b = B2T;
        m = M2T;
    }else if (ratio <= K3T){
        b = B3T;
        m = M3T;
    }else if (ratio <= K4T){
        b = B4T;
        m = M4T;
    }else if (ratio <= K5T){
        b = B5T;
        m = M5T;
    }else if (ratio <= K6T){
        b = B6T;
        m = M6T;
    }else if (ratio <= K7T){
        b = B7T;
        m = M7T;
    }else if (ratio > K8T){
        b = B8T;
        m = M8T;
    }

    temp = ((channel0 * b) - (channel1 * m));
    if (temp < 0)
        temp = 0;
    temp += (1 << (LUX_SCALE - 1));
    // strip off fractional portion
    *value = temp >> LUX_SCALE;

    return LE_OK;
}

static void SampleDigitalLight(
    psensor_Ref_t ref,
    void *context)
{
    uint16_t sample;

    le_result_t result = tsl2561DigitalLight_Read(&sample);

    if (result == LE_OK)
    {
        psensor_PushNumeric(ref, 0 /* now */, sample);
    }
    else
    {
        LE_ERROR("Failed to read sensor (%s).", LE_RESULT_TXT(result));
    }
}

COMPONENT_INIT
{
    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_POWER_UP, 2);
    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_TIMING, 2);
    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_INTERUPT, 2);
    i2cSendBytes(tls2561_sensor_i2c_bus, TSL2561_I2C_ADDR, TSL2561_POWER_DOWN, 2);

    psensor_Create("tsl2561_digital_light", DHUBIO_DATA_TYPE_NUMERIC, "lux", SampleDigitalLight, NULL);
}
