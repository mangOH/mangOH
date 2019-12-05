#include "legato.h"
#include "interfaces.h"
#include <glib.h>
#include "json.h"
#include "org.bluez.Adapter1.h"
#include "org.bluez.Device1.h"
#include "org.bluez.GattService1.h"
#include "org.bluez.GattCharacteristic1.h"
#include "org.bluez.GattDescriptor1.h"

#define SERVICE_UUID_SIMPLE_KEYS             "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_UUID_SIMPLE_KEYS_DATA "0000ffe1-0000-1000-8000-00805f9b34fb"

#define SERVICE_UUID_IR_TEMP                 "f000aa00-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_DATA     "f000aa01-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_CONFIG   "f000aa02-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_PERIOD   "f000aa03-0451-4000-b000-000000000000"

#define SERVICE_UUID_HUMIDITY                "f000aa20-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_DATA    "f000aa21-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_CONFIG  "f000aa22-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_PERIOD  "f000aa23-0451-4000-b000-000000000000"

#define SERVICE_UUID_PRESSURE                "f000aa40-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_PRESSURE_DATA    "f000aa41-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_PRESSURE_CONFIG  "f000aa42-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_PRESSURE_PERIOD  "f000aa44-0451-4000-b000-000000000000"

#define SERVICE_UUID_IO                      "f000aa64-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IO_DATA          "f000aa65-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IO_CONFIG        "f000aa66-0451-4000-b000-000000000000"

#define SERVICE_UUID_LIGHT                   "f000aa70-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_LIGHT_DATA       "f000aa71-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_LIGHT_CONFIG     "f000aa72-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_LIGHT_PERIOD     "f000aa73-0451-4000-b000-000000000000"

#define SERVICE_UUID_IMU                     "f000aa80-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IMU_DATA         "f000aa81-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IMU_CONFIG       "f000aa82-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IMU_PERIOD       "f000aa83-0451-4000-b000-000000000000"

#define RES_PATH_SIMPLE_KEYS_USER_VALUE      "simpleKeys/user/value"
#define RES_PATH_SIMPLE_KEYS_POWER_VALUE     "simpleKeys/power/value"
#define RES_PATH_SIMPLE_KEYS_REED_VALUE      "simpleKeys/reed/value"

#define RES_PATH_IR_TEMPERATURE_VALUE        "irTemperature/value"
#define RES_PATH_IR_TEMPERATURE_PERIOD       "irTemperature/period"
#define RES_PATH_IR_TEMPERATURE_ENABLE       "irTemperature/enable"

#define RES_PATH_HUMIDITY_VALUE              "humidity/value"
#define RES_PATH_HUMIDITY_PERIOD             "humidity/period"
#define RES_PATH_HUMIDITY_ENABLE             "humidity/enable"

#define RES_PATH_PRESSURE_VALUE              "pressure/value"
#define RES_PATH_PRESSURE_PERIOD             "pressure/period"
#define RES_PATH_PRESSURE_ENABLE             "pressure/enable"

#define RES_PATH_IO_VALUE                    "io/value"

#define RES_PATH_LIGHT_VALUE                 "light/value"
#define RES_PATH_LIGHT_PERIOD                "light/period"
#define RES_PATH_LIGHT_ENABLE                "light/enable"

#define RES_PATH_IMU_VALUE                   "imu/value"
#define RES_PATH_IMU_PERIOD                  "imu/period"
#define RES_PATH_IMU_ENABLE                  "imu/enable"
#define RES_PATH_IMU_GYRO_X_ENABLE           "imu/gyro/x/enable"
#define RES_PATH_IMU_GYRO_Y_ENABLE           "imu/gyro/y/enable"
#define RES_PATH_IMU_GYRO_Z_ENABLE           "imu/gyro/z/enable"
#define RES_PATH_IMU_ACCEL_X_ENABLE          "imu/accel/x/enable"
#define RES_PATH_IMU_ACCEL_Y_ENABLE          "imu/accel/y/enable"
#define RES_PATH_IMU_ACCEL_Z_ENABLE          "imu/accel/z/enable"
#define RES_PATH_IMU_ACCEL_WOM               "imu/accel/wakeOnMotion"
#define RES_PATH_IMU_ACCEL_RANGE             "imu/accel/range"
#define RES_PATH_IMU_MAG_ENABLE              "imu/mag/enable"

#define IR_TEMP_PERIOD_MIN 0.3
#define IR_TEMP_PERIOD_MAX 2.55

#define PRESSURE_PERIOD_MIN 0.1
#define PRESSURE_PERIOD_MAX 2.55

#define HUMIDITY_PERIOD_MIN 0.1
#define HUMIDITY_PERIOD_MAX 2.55

#define LIGHT_PERIOD_MIN 0.1
#define LIGHT_PERIOD_MAX 2.55

#define IMU_PERIOD_MIN 0.1
#define IMU_PERIOD_MAX 2.55

static GDBusObjectManager *BluezObjectManager = NULL;
static BluezAdapter1 *AdapterInterface = NULL;
static BluezDevice1 *SensorTagDeviceInterface = NULL;

static BluezGattCharacteristic1 *SimpleKeysCharacteristicData = NULL;

static BluezGattCharacteristic1 *IRTemperatureCharacteristicData = NULL;
static BluezGattCharacteristic1 *IRTemperatureCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *IRTemperatureCharacteristicPeriod = NULL;

static BluezGattCharacteristic1 *HumidityCharacteristicData = NULL;
static BluezGattCharacteristic1 *HumidityCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *HumidityCharacteristicPeriod = NULL;

static BluezGattCharacteristic1 *PressureCharacteristicData = NULL;
static BluezGattCharacteristic1 *PressureCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *PressureCharacteristicPeriod = NULL;

static BluezGattCharacteristic1 *IOCharacteristicData = NULL;
static BluezGattCharacteristic1 *IOCharacteristicConfig = NULL;

static BluezGattCharacteristic1 *LightCharacteristicData = NULL;
static BluezGattCharacteristic1 *LightCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *LightCharacteristicPeriod = NULL;

static BluezGattCharacteristic1 *ImuCharacteristicData = NULL;
static BluezGattCharacteristic1 *ImuCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *ImuCharacteristicPeriod = NULL;

struct Characteristic
{
    const gchar *uuid;
    BluezGattCharacteristic1 **proxy;
};

struct Service
{
    const gchar *name;
    const gchar *uuid;
    gchar *objectPath;
    const struct Characteristic *characteristics;
};

static const struct Characteristic SimpleKeysCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_SIMPLE_KEYS_DATA,
        .proxy = &SimpleKeysCharacteristicData,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic IRTemperatureCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_IR_TEMP_DATA,
        .proxy = &IRTemperatureCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_IR_TEMP_CONFIG,
        .proxy = &IRTemperatureCharacteristicConfig,
    },
    {
        .uuid = CHARACTERISTIC_UUID_IR_TEMP_PERIOD,
        .proxy = &IRTemperatureCharacteristicPeriod,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic HumidityCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_HUMIDITY_DATA,
        .proxy = &HumidityCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_HUMIDITY_CONFIG,
        .proxy = &HumidityCharacteristicConfig,
    },
    {
        .uuid = CHARACTERISTIC_UUID_HUMIDITY_PERIOD,
        .proxy = &HumidityCharacteristicPeriod,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic PressureCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_PRESSURE_DATA,
        .proxy = &PressureCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_PRESSURE_CONFIG,
        .proxy = &PressureCharacteristicConfig,
    },
    {
        .uuid = CHARACTERISTIC_UUID_PRESSURE_PERIOD,
        .proxy = &PressureCharacteristicPeriod,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic IOCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_IO_DATA,
        .proxy = &IOCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_IO_CONFIG,
        .proxy = &IOCharacteristicConfig,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic LightCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_LIGHT_DATA,
        .proxy = &LightCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_LIGHT_CONFIG,
        .proxy = &LightCharacteristicConfig,
    },
    {
        .uuid = CHARACTERISTIC_UUID_LIGHT_PERIOD,
        .proxy = &LightCharacteristicPeriod,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static const struct Characteristic ImuCharacteristics[] =
{
    {
        .uuid = CHARACTERISTIC_UUID_IMU_DATA,
        .proxy = &ImuCharacteristicData,
    },
    {
        .uuid = CHARACTERISTIC_UUID_IMU_CONFIG,
        .proxy = &ImuCharacteristicConfig,
    },
    {
        .uuid = CHARACTERISTIC_UUID_IMU_PERIOD,
        .proxy = &ImuCharacteristicPeriod,
    },
    {
        .uuid = NULL,
        .proxy = NULL,
    },
};

static struct Service Services[] =
{
    {
        .name = "Simple Keys",
        .uuid = SERVICE_UUID_SIMPLE_KEYS,
        .characteristics = SimpleKeysCharacteristics,
    },
    {
        .name = "IR Temperature",
        .uuid = SERVICE_UUID_IR_TEMP,
        .characteristics = IRTemperatureCharacteristics,
    },
    {
        .name = "Humidity",
        .uuid = SERVICE_UUID_HUMIDITY,
        .characteristics = HumidityCharacteristics,
    },
    {
        .name = "Pressure",
        .uuid = SERVICE_UUID_PRESSURE,
        .characteristics = PressureCharacteristics,
    },
    {
        .name = "IO",
        .uuid = SERVICE_UUID_IO,
        .characteristics = IOCharacteristics,
    },
    {
        .name = "Light",
        .uuid = SERVICE_UUID_LIGHT,
        .characteristics = LightCharacteristics,
    },
    {
        .name = "IMU",
        .uuid = SERVICE_UUID_IMU,
        .characteristics = ImuCharacteristics,
    },
};

enum PeriodValidity
{
    PERIOD_VALIDITY_OK,
    PERIOD_VALIDITY_TOO_SHORT,
    PERIOD_VALIDITY_TOO_LONG,
};

static enum ApplicationState
{
    APP_STATE_INIT,
    APP_STATE_SEARCHING_FOR_ADAPTER,
    APP_STATE_POWERING_ON_ADAPTER,
    APP_STATE_SEARCHING_FOR_SENSORTAG,
    APP_STATE_SEARCHING_FOR_ATTRIBUTES,
    APP_STATE_SAMPLING
} AppState = APP_STATE_INIT;

static struct
{
    struct
    {
        bool userPressed;
        bool powerPressed;
        bool reedActivated;
    } simpleKeys;
    struct
    {
        bool enable;
        double period;
    } irTemp;
    struct
    {
        bool enable;
        double period;
    } humidity;
    struct
    {
        bool enable;
        double period;
    } pressure;
    struct
    {
        bool redLed;
        bool greenLed;
        bool buzzer;
    } io;
    struct
    {
        bool enable;
        double period;
    } light;
    struct
    {
        bool enableGlobal;
        bool enableGyroX;
        bool enableGyroY;
        bool enableGyroZ;
        bool enableAccelX;
        bool enableAccelY;
        bool enableAccelZ;
        bool enableMag;
        bool wakeOnMotion;
        uint8_t accelerometerRange;
        double period;
    } imu;
} DHubState;

GType BluezProxyTypeFunc
(
    GDBusObjectManagerClient *manager,
    const gchar *objectPath,
    const gchar *interfaceName,
    gpointer userData
)
{
    LE_DEBUG("proxy ctor: path=%s, intf=%s", objectPath, interfaceName);
    if (interfaceName == NULL)
    {
        return g_dbus_object_proxy_get_type();
    }

    if (g_strcmp0(interfaceName, "org.bluez.Adapter1") == 0)
    {
        return BLUEZ_TYPE_ADAPTER1_PROXY;
    }
    else if (g_strcmp0(interfaceName, "org.bluez.Device1") == 0)
    {
        return BLUEZ_TYPE_DEVICE1_PROXY;
    }
    else if (g_strcmp0(interfaceName, "org.bluez.GattService1") == 0)
    {
        return BLUEZ_TYPE_GATT_SERVICE1_PROXY;
    }
    else if (g_strcmp0(interfaceName, "org.bluez.GattCharacteristic1") == 0)
    {
        return BLUEZ_TYPE_GATT_CHARACTERISTIC1_PROXY;
    }
    else if (g_strcmp0(interfaceName, "org.bluez.GattDescriptor1") == 0)
    {
        return BLUEZ_TYPE_GATT_DESCRIPTOR1_PROXY;
    }

    return g_dbus_proxy_get_type();
}

static gboolean LegatoFdHandler
(
    GIOChannel *source,
    GIOCondition condition,
    gpointer data
)
{
    while (true)
    {
        le_result_t r = le_event_ServiceLoop();
        if (r == LE_WOULD_BLOCK)
        {
            // All of the work is done, so break out
            break;
        }
        LE_ASSERT_OK(r);
    }

    return TRUE;
}


static BluezDevice1 *TryCreateSensorTagDeviceProxy
(
    GDBusObject *object
)
{
    BluezDevice1* dev = BLUEZ_DEVICE1(g_dbus_object_get_interface(object, "org.bluez.Device1"));
    if (dev != NULL)
    {
        const gchar *deviceName = bluez_device1_get_name(dev);
        if (g_strcmp0("CC2650 SensorTag", deviceName) != 0 &&
            g_strcmp0("CC1350 SensorTag", deviceName) != 0)
        {
            // Not a match, so dispose of the object
            g_clear_object(&dev);
        }
    }
    return dev;
}

/*
 * All services in the SensorTag use the same "period" scheme. It's a single unsigned byte
 * characteristic measured in 10ms increments.
 */
static void WritePeriod
(
    double period,
    BluezGattCharacteristic1 *periodCharacteristic
)
{
    GError *error = NULL;
    // Register is in units of 10 ms
    const uint8_t periodReg[] = {(uint8_t)(period * 100)};
    bluez_gatt_characteristic1_call_write_value_sync(
        periodCharacteristic,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, periodReg, NUM_ARRAY_MEMBERS(periodReg), sizeof(periodReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing sampling period - %s", error->message);
}

/*
 * For almost all services on the SensorTag, the configuration characteristic expects a single byte
 * where 0x01 means enable and 0x00 means disable.
 */
static void WriteBasicConfig
(
    bool enable,
    BluezGattCharacteristic1 *configCharacteristic
)
{
    GError *error = NULL;
    const uint8_t configReg[] = {enable ? 0x01 : 0x00};
    bluez_gatt_characteristic1_call_write_value_sync(
        configCharacteristic,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, configReg, NUM_ARRAY_MEMBERS(configReg), sizeof(configReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing config - %s", error->message);
}

static enum PeriodValidity ValidatePeriod
(
    double period,
    double minPeriod,
    double maxPeriod
)
{
    if (period < minPeriod)
    {
        return PERIOD_VALIDITY_TOO_SHORT;
    }

    if (period > maxPeriod)
    {
        return PERIOD_VALIDITY_TOO_LONG;
    }

    return PERIOD_VALIDITY_OK;
}


static void SimpleKeysDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 1, "Expected a value of size 1");
        const bool userPressed = ((valArray[0] & (1 << 0)) != 0);
        const bool powerPressed = ((valArray[0] & (1 << 1)) != 0);
        const bool reedActivated = ((valArray[0] & (1 << 2)) != 0);
        g_variant_unref(value);
        LE_DEBUG(
            "Received value - userPressed=%d, powerPressed=%d, reedActivated=%d",
            userPressed,
            powerPressed,
            reedActivated);
        if (userPressed != DHubState.simpleKeys.userPressed)
        {
            DHubState.simpleKeys.userPressed = userPressed;
            io_PushBoolean(RES_PATH_SIMPLE_KEYS_USER_VALUE, IO_NOW, userPressed);
        }
        if (powerPressed != DHubState.simpleKeys.powerPressed)
        {
            DHubState.simpleKeys.powerPressed = powerPressed;
            io_PushBoolean(RES_PATH_SIMPLE_KEYS_POWER_VALUE, IO_NOW, powerPressed);
        }
        if (reedActivated != DHubState.simpleKeys.reedActivated)
        {
            DHubState.simpleKeys.reedActivated = reedActivated;
            io_PushBoolean(RES_PATH_SIMPLE_KEYS_REED_VALUE, IO_NOW, reedActivated);
        }
    }
}


static void IrTemperatureDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 4, "Expected a value of size 4");
        const double divider = 128.0;
        const double objTemp = ((valArray[0] << 0) + (valArray[1] << 8)) / divider;
        const double ambTemp = ((valArray[2] << 0) + (valArray[3] << 8)) / divider;
        g_variant_unref(value);
        LE_DEBUG("Received value - objTemp=%f, ambTemp=%f", objTemp, ambTemp);
        char json[64];
        int res = snprintf(json, sizeof(json), "{ \"sensorDie\": %.3lf, \"object\": %.3lf }", ambTemp, objTemp);
        LE_ASSERT(res > 0 && res < sizeof(json));
        io_PushJson(RES_PATH_IR_TEMPERATURE_VALUE, IO_NOW, json);
    }
}


static void HumidityDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 4, "Expected a value of size 4");
        const uint16_t rawTemperature = ((valArray[0] << 0) | (valArray[1] << 8));
        const uint16_t rawHumidity = ((valArray[2] << 0) | (valArray[3] << 8));
        // temperature in degrees celcius
        const double temperature = ((((double)rawTemperature) / (1 << 16)) * 165) - 40;
        // relative humidity as percentage
        const double humidity = (((double)(rawHumidity & ~0x0003)) / (1 << 16)) * 100;
        g_variant_unref(value);
        LE_DEBUG("Received humidity data - temperature=%f, humidity=%f%%", temperature, humidity);
        char json[64];
        int res = snprintf(json, sizeof(json), "{ \"temperature\": %.3lf, \"humidity\": %.3lf }", temperature, humidity);
        LE_ASSERT(res > 0 && res < sizeof(json));
        io_PushJson(RES_PATH_HUMIDITY_VALUE, IO_NOW, json);
    }
}


static void PressureDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 6, "Expected a value of size 6");
        const uint32_t rawTemperature = ((valArray[0] << 0) | (valArray[1] << 8) | (valArray[2] << 16));
        const uint16_t rawPressure = ((valArray[3] << 0) | (valArray[4] << 8) | (valArray[5] << 16));
        // temperature in degrees celcius
        /*
         * TODO: The SensorTag wiki page says that the raw temperature is an unsigned 24-bit value
         * that when divided by 100 is the temperature in celcius. This seems strange since it means
         * that the temperature can never be below 0.
         */
        const double temperature = ((double)rawTemperature) / 100;
        // pressure is in hectopascal (hPa)
        const double pressure = ((double)rawPressure) / 100;
        g_variant_unref(value);
        LE_DEBUG("Received pressure data - temperature=%f, pressure=%f%%", temperature, pressure);
        char json[64];
        int res = snprintf(json, sizeof(json), "{ \"temperature\": %.3lf, \"pressure\": %.3lf }", temperature, pressure);
        LE_ASSERT(res > 0 && res < sizeof(json));
        io_PushJson(RES_PATH_PRESSURE_VALUE, IO_NOW, json);
    }
}


static void IOSetValue
(
    void
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    GError *error = NULL;
    const uint8_t dataReg[] = {
        (DHubState.io.redLed << 0) |
        (DHubState.io.greenLed << 1) |
        (DHubState.io.buzzer << 2)
    };
    bluez_gatt_characteristic1_call_write_value_sync(
        IOCharacteristicData,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, dataReg, NUM_ARRAY_MEMBERS(dataReg), sizeof(dataReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing IO data - %s", error->message);
}

static void IOSetConfig
(
    void
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    GError *error = NULL;
    const uint8_t configReg[] = { 1 }; // 1 means control manually
    bluez_gatt_characteristic1_call_write_value_sync(
        IOCharacteristicConfig,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, configReg, NUM_ARRAY_MEMBERS(configReg), sizeof(configReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing IO config - %s", error->message);
}


static void LightDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 2, "Expected a value of size 2");
        const uint16_t rawLightReg = ((valArray[0] << 0) | (valArray[1] << 8));
        const uint16_t mantissa = (rawLightReg & 0x0FFF);
        const uint16_t exponent = (rawLightReg >> 12);
        const uint16_t mult = (1 << exponent);
        const double lux = (mantissa * mult) / 100.0;
        LE_DEBUG("Received light data - lux=%f", lux);
        io_PushNumeric(RES_PATH_LIGHT_VALUE, IO_NOW, lux);
    }
}


static void ImuDataPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    GVariant *value = g_variant_lookup_value(changedProperties, "Value", G_VARIANT_TYPE_BYTESTRING);
    if (value != NULL)
    {
        gsize nElements;
        const uint8_t *valArray = g_variant_get_fixed_array(value, &nElements, sizeof(uint8_t));
        LE_FATAL_IF(nElements != 18, "Expected a value of size 18");

        // raw
        const int16_t gyroRawX  = ((valArray[0]  << 0) | (valArray[1]  << 8));
        const int16_t gyroRawY  = ((valArray[2]  << 0) | (valArray[3]  << 8));
        const int16_t gyroRawZ  = ((valArray[4]  << 0) | (valArray[5]  << 8));
        const int16_t accelRawX = ((valArray[6]  << 0) | (valArray[7]  << 8));
        const int16_t accelRawY = ((valArray[8]  << 0) | (valArray[9]  << 8));
        const int16_t accelRawZ = ((valArray[10] << 0) | (valArray[11] << 8));
        const int16_t magRawX   = ((valArray[12] << 0) | (valArray[13] << 8));
        const int16_t magRawY   = ((valArray[14] << 0) | (valArray[15] << 8));
        const int16_t magRawZ   = ((valArray[16] << 0) | (valArray[17] << 8));

        // converted
        const double gyroX = ((double)gyroRawX) * 500.0 / (1 << 16);
        const double gyroY = ((double)gyroRawY) * 500.0 / (1 << 16);
        const double gyroZ = ((double)gyroRawZ) * 500.0 / (1 << 16);

        const double accelX =
            ((double)accelRawX) / (1 << (16 - (1 + DHubState.imu.accelerometerRange)));
        const double accelY =
            ((double)accelRawY) / (1 << (16 - (1 + DHubState.imu.accelerometerRange)));
        const double accelZ =
            ((double)accelRawZ) / (1 << (16 - (1 + DHubState.imu.accelerometerRange)));

        const double magX = (double)magRawX;
        const double magY = (double)magRawY;
        const double magZ = (double)magRawZ;

        LE_DEBUG(
            "Received IMU data - gyro(%lf,%lf,%lf) accel(%lf,%lf,%lf) mag(%lf,%lf,%lf)",
            gyroX, gyroY, gyroZ, accelX, accelY, accelZ, magX, magY, magZ);
        char json[256];
        int res = snprintf(
            json,
            sizeof(json),
            "{ "
            "\"gyro\": { \"x\": %.2lf,  \"y\": %.2lf, \"z\": %.2lf }, "
            "\"accel\": { \"x\": %.2lf,  \"y\": %.2lf, \"z\": %.2lf }, "
            "\"mag\": { \"x\": %.1lf,  \"y\": %.1lf, \"z\": %.1lf } "
            "}",
            gyroX, gyroY, gyroZ, accelX, accelY, accelZ, magX, magY, magZ);
        LE_ASSERT(res > 0 && res < sizeof(json));
        io_PushJson(RES_PATH_IMU_VALUE, IO_NOW, json);
    }
}


static void WriteImuConfig(bool forceDisable)
{
    uint8_t configReg[2] = {0, 0}; // means disable
    if (DHubState.imu.enableGlobal && !forceDisable)
    {
        configReg[0] = (
            (DHubState.imu.enableGyroX  << 0) |
            (DHubState.imu.enableGyroY  << 1) |
            (DHubState.imu.enableGyroZ  << 2) |
            (DHubState.imu.enableAccelX << 3) |
            (DHubState.imu.enableAccelY << 4) |
            (DHubState.imu.enableAccelZ << 5) |
            (DHubState.imu.enableMag    << 6) |
            (DHubState.imu.wakeOnMotion << 7));
        configReg[1] = DHubState.imu.accelerometerRange;
    }
    GError *error = NULL;
    bluez_gatt_characteristic1_call_write_value_sync(
        ImuCharacteristicConfig,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, configReg, NUM_ARRAY_MEMBERS(configReg), sizeof(configReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing config - %s", error->message);
}

static void AllAttributesFoundHandler
(
    void
)
{
    AppState = APP_STATE_SAMPLING;
    GError *error = NULL;

    if (ValidatePeriod(DHubState.irTemp.period, IR_TEMP_PERIOD_MIN, IR_TEMP_PERIOD_MAX) ==
        PERIOD_VALIDITY_OK)
    {
        WritePeriod(DHubState.irTemp.period, IRTemperatureCharacteristicPeriod);
        WriteBasicConfig(DHubState.irTemp.enable, IRTemperatureCharacteristicConfig);
    }
    else
    {
        WriteBasicConfig(false, IRTemperatureCharacteristicConfig);
    }

    if (ValidatePeriod(DHubState.humidity.period, HUMIDITY_PERIOD_MIN, HUMIDITY_PERIOD_MAX) ==
        PERIOD_VALIDITY_OK)
    {
        WritePeriod(DHubState.humidity.period, HumidityCharacteristicPeriod);
        WriteBasicConfig(DHubState.humidity.enable, HumidityCharacteristicConfig);
    }
    else
    {
        WriteBasicConfig(false, HumidityCharacteristicConfig);
    }

    if (ValidatePeriod(DHubState.pressure.period, PRESSURE_PERIOD_MIN, PRESSURE_PERIOD_MAX) ==
        PERIOD_VALIDITY_OK)
    {
        WritePeriod(DHubState.pressure.period, PressureCharacteristicPeriod);
        WriteBasicConfig(DHubState.pressure.enable, PressureCharacteristicConfig);
    }
    else
    {
        WriteBasicConfig(false, PressureCharacteristicConfig);
    }

    if (ValidatePeriod(DHubState.imu.period, IMU_PERIOD_MIN, IMU_PERIOD_MAX) ==
        PERIOD_VALIDITY_OK)
    {
        WritePeriod(DHubState.imu.period, ImuCharacteristicPeriod);
        WriteImuConfig(false);
    }
    else
    {
        WriteImuConfig(true);
    }

    IOSetConfig();
    IOSetValue();

    g_signal_connect(
        SimpleKeysCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(SimpleKeysDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        SimpleKeysCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);

    g_signal_connect(
        IRTemperatureCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(IrTemperatureDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        IRTemperatureCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);

    g_signal_connect(
        HumidityCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(HumidityDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        HumidityCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);

    g_signal_connect(
        PressureCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(PressureDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        PressureCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);

    g_signal_connect(
        LightCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(LightDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        LightCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);

    g_signal_connect(
        ImuCharacteristicData,
        "g-properties-changed",
        G_CALLBACK(ImuDataPropertiesChangedHandler),
        NULL);
    bluez_gatt_characteristic1_call_start_notify_sync(
        ImuCharacteristicData,
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while calling StartNotify - %s", error->message);
}

static bool TryProcessAsService
(
    GDBusObject *object,
    const gchar *objectPath
)
{
    bool found = false;
    BluezGattService1 *serviceProxy =
        BLUEZ_GATT_SERVICE1(g_dbus_object_get_interface(object, "org.bluez.GattService1"));
    if (serviceProxy == NULL)
    {
        return false;
    }

    const gchar *serviceUuid = bluez_gatt_service1_get_uuid(serviceProxy);
    for (size_t i = 0; i < NUM_ARRAY_MEMBERS(Services); i++)
    {
        if (g_strcmp0(serviceUuid, Services[i].uuid) == 0)
        {
            LE_DEBUG("%s service found at: %s", Services[i].name, objectPath);
            LE_ASSERT(Services[i].objectPath == NULL);
            Services[i].objectPath = g_strdup(objectPath);
            found = true;
            break;
        }
    }
    g_clear_object(&serviceProxy);

    return found;
}

static bool TryProcessAsCharacteristic
(
    GDBusObject *object,
    const gchar *objectPath
)
{
    BluezGattCharacteristic1 *characteristicProxy = BLUEZ_GATT_CHARACTERISTIC1(
        g_dbus_object_get_interface(object, "org.bluez.GattCharacteristic1"));
    if (characteristicProxy == NULL)
    {
        return false;
    }
    const gchar *characteristicUuid = bluez_gatt_characteristic1_get_uuid(characteristicProxy);

    for (size_t serviceIdx = 0; serviceIdx < NUM_ARRAY_MEMBERS(Services); serviceIdx++)
    {
        const bool charInService = (
            g_strcmp0(
                bluez_gatt_characteristic1_get_service(characteristicProxy),
                Services[serviceIdx].objectPath) == 0);
        if (charInService)
        {
            for (const struct Characteristic *characteristic = Services[serviceIdx].characteristics;
                 characteristic->uuid != NULL;
                 characteristic++)
            {
                if (g_strcmp0(characteristic->uuid, characteristicUuid) == 0)
                {
                    LE_ASSERT(*(characteristic->proxy) == NULL);
                    LE_DEBUG(
                        "service %s characteristic %s found at %s",
                        Services[serviceIdx].name,
                        characteristic->uuid,
                        objectPath);
                    *(characteristic->proxy) = characteristicProxy;
                    return true;
                }
            }
            break;
        }
    }

    g_clear_object(&characteristicProxy);
    return false;
}


static void TryProcessAsAttribute
(
    GDBusObject *object,
    const gchar *devicePath
)
{
    gchar *unknownObjectPath = NULL;
    g_object_get(object, "g-object-path", &unknownObjectPath, NULL);
    const bool childOfDevice = g_str_has_prefix(unknownObjectPath, devicePath);
    if (!childOfDevice)
    {
        goto done;
    }

    bool missingCharacteristic = false;
    if (TryProcessAsService(object, unknownObjectPath))
    {
        // Services are discovered before their attributes, so there's no point in checking if all
        // attributes are found.
    }
    else if (TryProcessAsCharacteristic(object, unknownObjectPath))
    {
        for (size_t serviceIdx = 0;
             serviceIdx < NUM_ARRAY_MEMBERS(Services) && !missingCharacteristic;
             serviceIdx++)
        {
            for (const struct Characteristic *characteristic = Services[serviceIdx].characteristics;
                 characteristic->uuid != NULL;
                 characteristic++)
            {
                if (*(characteristic->proxy) == NULL)
                {
                    missingCharacteristic = true;
                    LE_DEBUG("Found missing characteristic: %s:%s", Services[serviceIdx].name, characteristic->uuid);
                    break;
                }
            }
        }

        if (!missingCharacteristic)
        {
            LE_DEBUG("Found all attributes");
            AllAttributesFoundHandler();
        }
    }

done:
    g_free(unknownObjectPath);
}


static void SensorTagFoundHandler(void)
{
    GError *error = NULL;
    LE_DEBUG("SensorTag found - calling Device1.Connect()");
    AppState = APP_STATE_SEARCHING_FOR_ATTRIBUTES;
    bluez_device1_call_connect_sync(SensorTagDeviceInterface, NULL, &error);
    LE_FATAL_IF(error != NULL, "Failed to connect to SensorTag - %s", error->message);

    gchar *devicePath = NULL;
    g_object_get(SensorTagDeviceInterface, "g-object-path", &devicePath, NULL);
    GList *bluezObjects = g_dbus_object_manager_get_objects(BluezObjectManager);
    for (GList *node = bluezObjects;
         node != NULL;
         node = node->next)
    {
        GDBusObject *obj = node->data;
        TryProcessAsAttribute(obj, devicePath);
    }
    g_list_free_full(bluezObjects, g_object_unref);
    g_free(devicePath);
}


static void BeginSensorTagSearch(void)
{
    AppState = APP_STATE_SEARCHING_FOR_SENSORTAG;
    GError *error = NULL;
    LE_DEBUG("Starting device discovery");
    bluez_adapter1_call_start_discovery_sync(AdapterInterface, NULL, &error);
    LE_FATAL_IF(error != NULL, "Couldn't start discovery - %s", error->message);

    GList *bluezObjects = g_dbus_object_manager_get_objects(BluezObjectManager);
    for (GList *node = bluezObjects;
         node != NULL && SensorTagDeviceInterface == NULL;
         node = node->next)
    {
        GDBusObject *obj = node->data;
        SensorTagDeviceInterface = TryCreateSensorTagDeviceProxy(obj);
        if (SensorTagDeviceInterface != NULL)
        {
            SensorTagFoundHandler();
        }
    }
    g_list_free_full(bluezObjects, g_object_unref);
}


static void AdapterPropertiesChangedHandler
(
    GDBusProxy *proxy,
    GVariant *changedProperties,
    GStrv invalidatedProperties,
    gpointer userData
)
{
    LE_DEBUG("%s - AppState=%d", __func__, AppState);
    if (AppState == APP_STATE_POWERING_ON_ADAPTER)
    {
        GVariant *poweredVal =
            g_variant_lookup_value(changedProperties, "Powered", G_VARIANT_TYPE_BOOLEAN);
        if (poweredVal != NULL)
        {
            gboolean powered = g_variant_get_boolean(poweredVal);
            g_variant_unref(poweredVal);
            LE_DEBUG("Adapter Powered property = %d", powered);
            if (powered)
            {
                BeginSensorTagSearch();
            }
        }
    }
}


static void AdapterFoundHandler(void)
{
    // Ensure the adapter is powered on
    if (!bluez_adapter1_get_powered(AdapterInterface))
    {
        AppState = APP_STATE_POWERING_ON_ADAPTER;
        LE_DEBUG("Adapter not powered - powering on");
        g_signal_connect(
            AdapterInterface,
            "g-properties-changed",
            G_CALLBACK(AdapterPropertiesChangedHandler),
            NULL);
        bluez_adapter1_set_powered(AdapterInterface, TRUE);
    }
    else
    {
        LE_DEBUG("Adapter already powered");
        BeginSensorTagSearch();
    }
}


static void BluezObjectAddedHandler
(
    GDBusObjectManager *manager,
    GDBusObject *object,
    gpointer userData
)
{
    switch (AppState)
    {
    case APP_STATE_SEARCHING_FOR_ADAPTER:
        AdapterInterface =
            BLUEZ_ADAPTER1(g_dbus_object_get_interface(object, "org.bluez.Adapter1"));
        if (AdapterInterface != NULL)
        {
            AdapterFoundHandler();
        }
        break;

    case APP_STATE_SEARCHING_FOR_SENSORTAG:
        {
            SensorTagDeviceInterface = TryCreateSensorTagDeviceProxy(object);
            if (SensorTagDeviceInterface != NULL)
            {
                SensorTagFoundHandler();
            }
        }
        break;

    case APP_STATE_SEARCHING_FOR_ATTRIBUTES:
        {
            gchar *devicePath = NULL;
            g_object_get(SensorTagDeviceInterface, "g-object-path", &devicePath, NULL);
            TryProcessAsAttribute(object, devicePath);
            g_free(devicePath);
        }
        break;

    default:
        LE_DEBUG(
            "Received \"object-added\" signal - object_path=%s, state=%d",
            g_dbus_object_get_object_path(object),
            AppState);
        break;
    }
}


static void BluezObjectRemovedHandler
(
    GDBusObjectManager *manager,
    GDBusObject *object,
    gpointer userData
)
{
    LE_DEBUG(
        "Received \"object-removed\" signal - object_path=%s",
        g_dbus_object_get_object_path(object));
}


static void SearchForAdapter(void)
{
    AppState = APP_STATE_SEARCHING_FOR_ADAPTER;
    GList *bluezObjects = g_dbus_object_manager_get_objects(BluezObjectManager);
    for (GList *node = bluezObjects; node != NULL && AdapterInterface == NULL; node = node->next)
    {
        GDBusObject *obj = node->data;
        AdapterInterface = BLUEZ_ADAPTER1(g_dbus_object_get_interface(obj, "org.bluez.Adapter1"));
    }
    g_list_free_full(bluezObjects, g_object_unref);

    if (AdapterInterface != NULL)
    {
        AdapterFoundHandler();
    }
}


static void BluezObjectManagerCreateCallback
(
    GObject *sourceObject,
    GAsyncResult *res,
    gpointer user_data)
{
    GError *error = NULL;
    BluezObjectManager = g_dbus_object_manager_client_new_for_bus_finish(res, &error);
    LE_FATAL_IF(error != NULL, "Couldn't create Bluez object manager - %s", error->message);

    g_signal_connect(BluezObjectManager, "object-added", G_CALLBACK(BluezObjectAddedHandler), NULL);
    g_signal_connect(
        BluezObjectManager, "object-removed", G_CALLBACK(BluezObjectRemovedHandler), NULL);

    SearchForAdapter();
}


static void GlibInit(void *deferredArg1, void *deferredArg2)
{
    GMainLoop *glibMainLoop = g_main_loop_new(NULL, FALSE);

    int legatoEventLoopFd = le_event_GetFd();
    GIOChannel *channel = g_io_channel_unix_new(legatoEventLoopFd);
    gpointer userData = NULL;
    g_io_add_watch(channel, G_IO_IN, LegatoFdHandler, userData);

    g_dbus_object_manager_client_new_for_bus(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
        "org.bluez",
        "/",
        BluezProxyTypeFunc,
        NULL,
        NULL,
        NULL,
        BluezObjectManagerCreateCallback,
        NULL);

    g_main_loop_run(glibMainLoop);

    LE_FATAL("GLib main loop has returned");
}

static void HandleIrTempEnablePush
(
    double timestamp,
    bool enable,
    void* contextPtr
)
{
    if (enable != DHubState.irTemp.enable)
    {
        DHubState.irTemp.enable = enable;
        if (AppState == APP_STATE_SAMPLING &&
            ValidatePeriod(DHubState.irTemp.period, IR_TEMP_PERIOD_MIN, IR_TEMP_PERIOD_MAX) ==
            PERIOD_VALIDITY_OK)
        {
            WriteBasicConfig(DHubState.irTemp.enable, IRTemperatureCharacteristicConfig);
        }
    }
}

static void HandleIrTempPeriodPush
(
    double timestamp,
    double period,
    void* contextPtr
)
{
    switch (ValidatePeriod(period, IR_TEMP_PERIOD_MIN, IR_TEMP_PERIOD_MAX))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting IR temperature sensor period to %lf: minimum period is %lf",
            period,
            IR_TEMP_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting IR temperature sensor period to %lf: maximum period is %lf",
            period,
            IR_TEMP_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.irTemp.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                WritePeriod(period, IRTemperatureCharacteristicPeriod);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.irTemp.enable)
                {
                    const bool oldPeriodValid =
                        ValidatePeriod(DHubState.irTemp.period, IR_TEMP_PERIOD_MIN, IR_TEMP_PERIOD_MAX) == PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        WriteBasicConfig(true, IRTemperatureCharacteristicConfig);
                    }
                }
            }
            DHubState.irTemp.period = period;
        }
        else
        {
            LE_DEBUG("IR temperature sensor period is already set to %lf", period);
        }
        break;

    default:
        LE_ASSERT(false);
        break;
    }
}

static void HandleHumidityEnablePush
(
    double timestamp,
    bool enable,
    void* contextPtr
)
{
    if (enable != DHubState.humidity.enable)
    {
        DHubState.humidity.enable = enable;
        if (AppState == APP_STATE_SAMPLING &&
            ValidatePeriod(DHubState.humidity.period, HUMIDITY_PERIOD_MIN, HUMIDITY_PERIOD_MAX) ==
            PERIOD_VALIDITY_OK)
        {
            WriteBasicConfig(DHubState.humidity.enable, HumidityCharacteristicConfig);
        }
    }
}

static void HandleHumidityPeriodPush
(
    double timestamp,
    double period,
    void* contextPtr
)
{
    switch (ValidatePeriod(period, HUMIDITY_PERIOD_MIN, HUMIDITY_PERIOD_MAX))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting Humidity sensor period to %lf: minimum period is %lf",
            period,
            HUMIDITY_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting Humidity sensor period to %lf: maximum period is %lf",
            period,
            HUMIDITY_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.humidity.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                WritePeriod(period, HumidityCharacteristicPeriod);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.humidity.enable)
                {
                    const bool oldPeriodValid =
                        ValidatePeriod(DHubState.humidity.period, HUMIDITY_PERIOD_MIN, HUMIDITY_PERIOD_MAX) ==
                        PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        WriteBasicConfig(true, HumidityCharacteristicConfig);
                    }
                }
            }
            DHubState.humidity.period = period;
        }
        else
        {
            LE_DEBUG("Humidity sensor period is already set to %lf", period);
        }
        break;

    default:
        LE_ASSERT(false);
        break;
    }
}

static void HandlePressureEnablePush
(
    double timestamp,
    bool enable,
    void* contextPtr
)
{
    if (enable != DHubState.pressure.enable)
    {
        DHubState.pressure.enable = enable;
        if (AppState == APP_STATE_SAMPLING &&
            ValidatePeriod(DHubState.pressure.period, PRESSURE_PERIOD_MIN, PRESSURE_PERIOD_MAX) ==
            PERIOD_VALIDITY_OK)
        {
            WriteBasicConfig(DHubState.pressure.enable, PressureCharacteristicConfig);
        }
    }
}

static void HandlePressurePeriodPush
(
    double timestamp,
    double period,
    void* contextPtr
)
{
    switch (ValidatePeriod(period, PRESSURE_PERIOD_MIN, PRESSURE_PERIOD_MAX))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting Pressure sensor period to %lf: minimum period is %lf",
            period,
            PRESSURE_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting Pressure sensor period to %lf: maximum period is %lf",
            period,
            PRESSURE_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.pressure.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                WritePeriod(period, PressureCharacteristicPeriod);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.pressure.enable)
                {
                    const bool oldPeriodValid =
                        ValidatePeriod(DHubState.pressure.period, PRESSURE_PERIOD_MIN, PRESSURE_PERIOD_MAX) ==
                        PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        WriteBasicConfig(true, PressureCharacteristicConfig);
                    }
                }
            }
            DHubState.pressure.period = period;
        }
        else
        {
            LE_DEBUG("Pressure sensor period is already set to %lf", period);
        }
        break;

    default:
        LE_ASSERT(false);
        break;
    }
}


static void HandleLightEnablePush
(
    double timestamp,
    bool enable,
    void* contextPtr
)
{
    if (enable != DHubState.light.enable)
    {
        DHubState.light.enable = enable;
        if (AppState == APP_STATE_SAMPLING &&
            ValidatePeriod(DHubState.light.period, LIGHT_PERIOD_MIN, LIGHT_PERIOD_MAX) == PERIOD_VALIDITY_OK)
        {
            WriteBasicConfig(DHubState.light.enable, LightCharacteristicConfig);
        }
    }
}

static void HandleLightPeriodPush
(
    double timestamp,
    double period,
    void* contextPtr
)
{
    switch (ValidatePeriod(period, LIGHT_PERIOD_MIN, LIGHT_PERIOD_MAX))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting Light sensor period to %lf: minimum period is %lf",
            period,
            LIGHT_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting Light sensor period to %lf: maximum period is %lf",
            period,
            LIGHT_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.light.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                WritePeriod(period, LightCharacteristicPeriod);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.light.enable)
                {
                    const bool oldPeriodValid =
                        ValidatePeriod(DHubState.light.period, LIGHT_PERIOD_MIN, LIGHT_PERIOD_MAX) ==
                        PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        WriteBasicConfig(true, LightCharacteristicConfig);
                    }
                }
            }
            DHubState.light.period = period;
        }
        else
        {
            LE_DEBUG("Light sensor period is already set to %lf", period);
        }
        break;

    default:
        LE_ASSERT(false);
        break;
    }
}

static void HandleImuEnablePush
(
    double timestamp,
    bool enable,
    void* context
)
{
    bool *toUpdate = context;
    if (*toUpdate != enable)
    {
        *toUpdate = enable;

        const bool enableGlobalChanged = (toUpdate == &DHubState.imu.enableGlobal);
        const bool periodValid =
            (ValidatePeriod(DHubState.imu.period, IMU_PERIOD_MIN, IMU_PERIOD_MAX) ==
             PERIOD_VALIDITY_OK);
        if (enableGlobalChanged)
        {
            WriteImuConfig(!periodValid);
        }
        else if (DHubState.imu.enableGlobal && periodValid)
        {
            WriteImuConfig(false);
        }
    }
}

static void HandleImuAccelRangePush
(
    double timestamp,
    double value,
    void *context
)
{
    uint8_t rangeRegVal;

    if (value == 16.0)
    {
        rangeRegVal = 3;
    }
    else if (value == 8.0)
    {
        rangeRegVal = 2;
    }
    else if (value == 4.0)
    {
        rangeRegVal = 1;
    }
    else if (value == 2.0)
    {
        rangeRegVal = 0;
    }
    else
    {
        LE_WARN("Received unsupported IMU accelerometer range (%lf)", value);
        return;
    }

    if (rangeRegVal != DHubState.imu.accelerometerRange)
    {
        const bool periodValid =
            (ValidatePeriod(DHubState.imu.period, IMU_PERIOD_MIN, IMU_PERIOD_MAX) ==
             PERIOD_VALIDITY_OK);
        if (DHubState.imu.enableGlobal && periodValid)
        {
            WriteImuConfig(false);
        }
    }
}

static void HandleImuPeriodPush
(
    double timestamp,
    double period,
    void* context
)
{
    switch (ValidatePeriod(period, IMU_PERIOD_MIN, IMU_PERIOD_MAX))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting Imu sensor period to %lf: minimum period is %lf",
            period,
            IMU_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting Imu sensor period to %lf: maximum period is %lf",
            period,
            IMU_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.imu.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                WritePeriod(period, ImuCharacteristicPeriod);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.imu.enableGlobal)
                {
                    const bool oldPeriodValid =
                        ValidatePeriod(DHubState.imu.period, IMU_PERIOD_MIN, IMU_PERIOD_MAX) ==
                        PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        WriteImuConfig(false);
                    }
                }
            }
            DHubState.imu.period = period;
        }
        else
        {
            LE_DEBUG("Imu sensor period is already set to %lf", period);
        }
        break;

    default:
        LE_ASSERT(false);
        break;
    }
}


static le_result_t ExtractBoolFromJson
(
    const char *jsonValue,
    const char *extractionSpec,
    bool *value
)
{
    char jsonBuffer[8]; // big enough for a bool
    json_DataType_t valueType;
    le_result_t res = json_Extract(
        jsonBuffer, sizeof(jsonBuffer), jsonValue, extractionSpec, &valueType);
    if (res != LE_OK)
    {
        LE_WARN("Couldn't extract %s from JSON", extractionSpec);
        return LE_BAD_PARAMETER;
    }
    if (valueType != JSON_TYPE_BOOLEAN)
    {
        LE_WARN("JSON member %s isn't a bool", extractionSpec);
        return LE_BAD_PARAMETER;
    }
    *value = json_ConvertToBoolean(jsonBuffer);
    return LE_OK;
}


static void HandleIOPush
(
    double timestamp,
    const char *json,
    void* contextPtr
)
{
    bool redLed;
    bool greenLed;
    bool buzzer;
    if (ExtractBoolFromJson(json, "redLed", &redLed) != LE_OK ||
        ExtractBoolFromJson(json, "greenLed", &greenLed) != LE_OK ||
        ExtractBoolFromJson(json, "buzzer", &buzzer) != LE_OK)
    {
        LE_WARN("IO push data rejected");
        return;
    }

    if (redLed != DHubState.io.redLed ||
        greenLed != DHubState.io.greenLed ||
        buzzer != DHubState.io.buzzer)
    {
        DHubState.io.redLed = redLed;
        DHubState.io.greenLed = greenLed;
        DHubState.io.buzzer = buzzer;
        if (AppState == APP_STATE_SAMPLING)
        {
            IOSetValue();
        }
    }
}


COMPONENT_INIT
{
    // Simple Keys
    LE_ASSERT_OK(io_CreateInput(RES_PATH_SIMPLE_KEYS_USER_VALUE, IO_DATA_TYPE_BOOLEAN, ""));
    LE_ASSERT_OK(io_CreateInput(RES_PATH_SIMPLE_KEYS_POWER_VALUE, IO_DATA_TYPE_BOOLEAN, ""));
    LE_ASSERT_OK(io_CreateInput(RES_PATH_SIMPLE_KEYS_REED_VALUE, IO_DATA_TYPE_BOOLEAN, ""));

    // IR Temperature
    LE_ASSERT_OK(io_CreateInput(RES_PATH_IR_TEMPERATURE_VALUE, IO_DATA_TYPE_JSON, ""));
    io_SetJsonExample(RES_PATH_IR_TEMPERATURE_VALUE, "{ \"sensorDie\": 26.4, \"object\": 19.3 }");

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IR_TEMPERATURE_PERIOD, IO_DATA_TYPE_NUMERIC, "seconds"));
    io_AddNumericPushHandler(RES_PATH_IR_TEMPERATURE_PERIOD, HandleIrTempPeriodPush, NULL);
    io_SetNumericDefault(RES_PATH_IR_TEMPERATURE_PERIOD, 0.0);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IR_TEMPERATURE_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IR_TEMPERATURE_ENABLE, HandleIrTempEnablePush, NULL);
    io_SetBooleanDefault(RES_PATH_IR_TEMPERATURE_ENABLE, false);

    // Humidity
    LE_ASSERT_OK(io_CreateInput(RES_PATH_HUMIDITY_VALUE, IO_DATA_TYPE_JSON, ""));
    io_SetJsonExample(RES_PATH_HUMIDITY_VALUE, "{ \"temperature\": 21.3, \"humidity\": 63.9 }");

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_HUMIDITY_PERIOD, IO_DATA_TYPE_NUMERIC, "seconds"));
    io_AddNumericPushHandler(RES_PATH_HUMIDITY_PERIOD, HandleHumidityPeriodPush, NULL);
    io_SetNumericDefault(RES_PATH_HUMIDITY_PERIOD, 0.0);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_HUMIDITY_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_HUMIDITY_ENABLE, HandleHumidityEnablePush, NULL);
    io_SetBooleanDefault(RES_PATH_HUMIDITY_ENABLE, false);

    // Pressure
    LE_ASSERT_OK(io_CreateInput(RES_PATH_PRESSURE_VALUE, IO_DATA_TYPE_JSON, ""));
    io_SetJsonExample(RES_PATH_PRESSURE_VALUE, "{ \"temperature\": 21.3, \"pressure\": 1001.7 }");

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_PRESSURE_PERIOD, IO_DATA_TYPE_NUMERIC, "seconds"));
    io_AddNumericPushHandler(RES_PATH_PRESSURE_PERIOD, HandlePressurePeriodPush, NULL);
    io_SetNumericDefault(RES_PATH_PRESSURE_PERIOD, 0.0);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_PRESSURE_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_PRESSURE_ENABLE, HandlePressureEnablePush, NULL);
    io_SetBooleanDefault(RES_PATH_PRESSURE_ENABLE, false);

    // IO
    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IO_VALUE, IO_DATA_TYPE_JSON, ""));
    io_AddJsonPushHandler(RES_PATH_IO_VALUE, HandleIOPush, NULL);
    io_SetJsonDefault(
        RES_PATH_IO_VALUE, "{ \"redLed\": false, \"greenLed\": false, \"buzzer\": false }");

    // Light
    LE_ASSERT_OK(io_CreateInput(RES_PATH_LIGHT_VALUE, IO_DATA_TYPE_NUMERIC, "lux"));

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_LIGHT_PERIOD, IO_DATA_TYPE_NUMERIC, "seconds"));
    io_AddNumericPushHandler(RES_PATH_LIGHT_PERIOD, HandleLightPeriodPush, NULL);
    io_SetNumericDefault(RES_PATH_LIGHT_PERIOD, 0.0);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_LIGHT_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_LIGHT_ENABLE, HandleLightEnablePush, NULL);
    io_SetBooleanDefault(RES_PATH_LIGHT_ENABLE, false);

    // IMU
    LE_ASSERT_OK(io_CreateInput(RES_PATH_IMU_VALUE, IO_DATA_TYPE_JSON, ""));
    io_SetJsonExample(
        RES_PATH_IMU_VALUE,
        "{ "
        "\"gyro\": { \"x\": 360.0,  \"y\": 180.0, \"z\": 90.0 }, "
        "\"accel\": { \"x\": 0.21,  \"y\": 0.13, \"z\": 1.0 }, "
        "\"mag\": { \"x\": 500.0,  \"y\": -312.7, \"z\": 45.3 } "
        "}");

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_PERIOD, IO_DATA_TYPE_NUMERIC, "seconds"));
    io_AddNumericPushHandler(RES_PATH_IMU_PERIOD, HandleImuPeriodPush, NULL);
    io_SetNumericDefault(RES_PATH_IMU_PERIOD, 0.0);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_ENABLE, HandleImuEnablePush, &DHubState.imu.enableGlobal);
    io_SetBooleanDefault(RES_PATH_IMU_ENABLE, false);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_GYRO_X_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_GYRO_X_ENABLE, HandleImuEnablePush, &DHubState.imu.enableGyroX);
    io_SetBooleanDefault(RES_PATH_IMU_GYRO_X_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_GYRO_Y_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_GYRO_Y_ENABLE, HandleImuEnablePush, &DHubState.imu.enableGyroY);
    io_SetBooleanDefault(RES_PATH_IMU_GYRO_Y_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_GYRO_Z_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_GYRO_Z_ENABLE, HandleImuEnablePush, &DHubState.imu.enableGyroZ);
    io_SetBooleanDefault(RES_PATH_IMU_GYRO_Z_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ACCEL_X_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_ACCEL_X_ENABLE, HandleImuEnablePush, &DHubState.imu.enableAccelX);
    io_SetBooleanDefault(RES_PATH_IMU_ACCEL_X_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ACCEL_Y_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_ACCEL_Y_ENABLE, HandleImuEnablePush, &DHubState.imu.enableAccelY);
    io_SetBooleanDefault(RES_PATH_IMU_ACCEL_Y_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ACCEL_Z_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_ACCEL_Z_ENABLE, HandleImuEnablePush, &DHubState.imu.enableAccelZ);
    io_SetBooleanDefault(RES_PATH_IMU_ACCEL_Z_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_MAG_ENABLE, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_MAG_ENABLE, HandleImuEnablePush, &DHubState.imu.enableMag);
    io_SetBooleanDefault(RES_PATH_IMU_MAG_ENABLE, true);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ACCEL_WOM, IO_DATA_TYPE_BOOLEAN, ""));
    io_AddBooleanPushHandler(RES_PATH_IMU_ACCEL_WOM, HandleImuEnablePush, &DHubState.imu.wakeOnMotion);
    io_SetBooleanDefault(RES_PATH_IMU_ACCEL_WOM, false);

    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IMU_ACCEL_RANGE, IO_DATA_TYPE_NUMERIC, "G"));
    io_AddNumericPushHandler(RES_PATH_IMU_ACCEL_RANGE, HandleImuAccelRangePush, NULL);
    io_SetNumericDefault(RES_PATH_IMU_ACCEL_RANGE, 16.0);

    le_event_QueueFunction(GlibInit, NULL, NULL);
}
