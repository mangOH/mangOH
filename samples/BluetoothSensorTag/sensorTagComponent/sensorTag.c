#include "legato.h"
#include "interfaces.h"
#include <glib.h>
#include "json.h"
#include "org.bluez.Adapter1.h"
#include "org.bluez.Device1.h"
#include "org.bluez.GattService1.h"
#include "org.bluez.GattCharacteristic1.h"
#include "org.bluez.GattDescriptor1.h"

#define SERVICE_UUID_IR_TEMP                                "f000aa00-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_DATA                    "f000aa01-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_CONFIG                  "f000aa02-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IR_TEMP_PERIOD                  "f000aa03-0451-4000-b000-000000000000"

#define SERVICE_UUID_HUMIDITY                               "f000aa20-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_DATA                   "f000aa21-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_CONFIG                 "f000aa22-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_HUMIDITY_PERIOD                 "f000aa23-0451-4000-b000-000000000000"

#define SERVICE_UUID_IO                                     "f000aa64-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IO_DATA                         "f000aa65-0451-4000-b000-000000000000"
#define CHARACTERISTIC_UUID_IO_CONFIG                       "f000aa66-0451-4000-b000-000000000000"

#define RES_PATH_IR_TEMPERATURE_VALUE "irTemperature/value"
#define RES_PATH_IR_TEMPERATURE_PERIOD "irTemperature/period"
#define RES_PATH_IR_TEMPERATURE_ENABLE "irTemperature/enable"

#define RES_PATH_HUMIDITY_VALUE  "humidity/value"
#define RES_PATH_HUMIDITY_PERIOD "humidity/period"
#define RES_PATH_HUMIDITY_ENABLE "humidity/enable"

#define RES_PATH_IO_VALUE "io/value"

#define IR_TEMP_PERIOD_MIN 0.3
#define IR_TEMP_PERIOD_MAX 2.55

#define HUMIDITY_PERIOD_MIN 0.1
#define HUMIDITY_PERIOD_MAX 2.55

static GDBusObjectManager *BluezObjectManager = NULL;
static BluezAdapter1 *AdapterInterface = NULL;
static BluezDevice1 *SensorTagDeviceInterface = NULL;

static gchar *IRTemperatureSensorServicePath = NULL;
static BluezGattCharacteristic1 *IRTemperatureCharacteristicData = NULL;
static BluezGattCharacteristic1 *IRTemperatureCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *IRTemperatureCharacteristicPeriod = NULL;

static gchar *HumiditySensorServicePath = NULL;
static BluezGattCharacteristic1 *HumidityCharacteristicData = NULL;
static BluezGattCharacteristic1 *HumidityCharacteristicConfig = NULL;
static BluezGattCharacteristic1 *HumidityCharacteristicPeriod = NULL;

static gchar *IOServicePath = NULL;
static BluezGattCharacteristic1 *IOCharacteristicData = NULL;
static BluezGattCharacteristic1 *IOCharacteristicConfig = NULL;

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
        bool redLed;
        bool greenLed;
        bool buzzer;
    } io;
} DHubState;


GType BluezProxyTypeFunc
(
    GDBusObjectManagerClient *manager,
    const gchar *objectPath,
    const gchar *interfaceName,
    gpointer userData
)
{
    LE_DEBUG("Handling request for objectPath=%s, interfaceName=%s", objectPath, interfaceName);
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


static bool IsCharacteristicInService
(
    const gchar *servicePath,
    BluezGattCharacteristic1 *characteristic
)
{
    const bool charInService = (
        g_strcmp0(bluez_gatt_characteristic1_get_service(characteristic), servicePath) == 0);

    return charInService;
}


static enum PeriodValidity ValidateIrTempPeriod
(
    double period
)
{
    if (period < IR_TEMP_PERIOD_MIN)
    {
        return PERIOD_VALIDITY_TOO_SHORT;
    }

    if (period > IR_TEMP_PERIOD_MAX)
    {
        return PERIOD_VALIDITY_TOO_LONG;
    }

    return PERIOD_VALIDITY_OK;
}

static void IrTemperatureSetEnable
(
    bool enable
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    GError *error = NULL;
    const uint8_t configReg[] = {enable ? 0x01 : 0x00};
    bluez_gatt_characteristic1_call_write_value_sync(
        IRTemperatureCharacteristicConfig,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, configReg, NUM_ARRAY_MEMBERS(configReg), sizeof(configReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing config - %s", error->message);
}

static void IrTemperatureSetPeriod
(
    double period
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    LE_ASSERT(period >= IR_TEMP_PERIOD_MIN && period <= IR_TEMP_PERIOD_MAX);
    GError *error = NULL;
    // Register is in units of 10 ms
    const uint8_t periodReg[] = {(uint8_t)(period * 100)};
    bluez_gatt_characteristic1_call_write_value_sync(
        IRTemperatureCharacteristicPeriod,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, periodReg, NUM_ARRAY_MEMBERS(periodReg), sizeof(periodReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing sampling period - %s", error->message);
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


static enum PeriodValidity ValidateHumidityPeriod
(
    double period
)
{
    if (period < HUMIDITY_PERIOD_MIN)
    {
        return PERIOD_VALIDITY_TOO_SHORT;
    }

    if (period > HUMIDITY_PERIOD_MAX)
    {
        return PERIOD_VALIDITY_TOO_LONG;
    }

    return PERIOD_VALIDITY_OK;
}

static void HumiditySetEnable
(
    bool enable
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    GError *error = NULL;
    const uint8_t configReg[] = {enable ? 0x01 : 0x00};
    bluez_gatt_characteristic1_call_write_value_sync(
        HumidityCharacteristicConfig,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, configReg, NUM_ARRAY_MEMBERS(configReg), sizeof(configReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing config - %s", error->message);
}

static void HumiditySetPeriod
(
    double period
)
{
    LE_ASSERT(AppState == APP_STATE_SAMPLING);
    LE_ASSERT(period >= HUMIDITY_PERIOD_MIN && period <= HUMIDITY_PERIOD_MAX);
    GError *error = NULL;
    // Register is in units of 10 ms
    const uint8_t periodReg[] = {(uint8_t)(period * 100)};
    bluez_gatt_characteristic1_call_write_value_sync(
        HumidityCharacteristicPeriod,
        g_variant_new_fixed_array(
            G_VARIANT_TYPE_BYTE, periodReg, NUM_ARRAY_MEMBERS(periodReg), sizeof(periodReg[0])),
        g_variant_new_array(G_VARIANT_TYPE("{sv}"), NULL, 0),
        NULL,
        &error);
    LE_FATAL_IF(error, "Failed while writing sampling period - %s", error->message);
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

static void AllAttributesFoundHandler
(
    void
)
{
    AppState = APP_STATE_SAMPLING;
    GError *error = NULL;

    if (ValidateIrTempPeriod(DHubState.irTemp.period) == PERIOD_VALIDITY_OK)
    {
        IrTemperatureSetPeriod(DHubState.irTemp.period);
        IrTemperatureSetEnable(DHubState.irTemp.enable);
    }
    else
    {
        IrTemperatureSetEnable(false);
    }

    if (ValidateHumidityPeriod(DHubState.humidity.period) == PERIOD_VALIDITY_OK)
    {
        HumiditySetPeriod(DHubState.humidity.period);
        HumiditySetEnable(DHubState.humidity.enable);
    }
    else
    {
        HumiditySetEnable(false);
    }

    IOSetConfig();
    IOSetValue();

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
}

static bool TryProcessAsService
(
    GDBusObject *object,
    const gchar *objectPath,
    const gchar *devicePath
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

    if (g_strcmp0(serviceUuid, SERVICE_UUID_IR_TEMP) == 0)
    {
        LE_DEBUG("IR temperature service found at: %s", objectPath);
        LE_ASSERT(IRTemperatureSensorServicePath == NULL);
        IRTemperatureSensorServicePath = g_strdup(objectPath);
        found = true;
        goto cleanup;
    }

    if (g_strcmp0(serviceUuid, SERVICE_UUID_HUMIDITY) == 0)
    {
        LE_DEBUG("Humidity service found at: %s", objectPath);
        LE_ASSERT(HumiditySensorServicePath == NULL);
        HumiditySensorServicePath = g_strdup(objectPath);
        found = true;
        goto cleanup;
    }

    if (g_strcmp0(serviceUuid, SERVICE_UUID_IO) == 0)
    {
        LE_DEBUG("IO service found at: %s", objectPath);
        LE_ASSERT(IOServicePath == NULL);
        IOServicePath = g_strdup(objectPath);
        found = true;
        goto cleanup;
    }

    LE_DEBUG("Ignoring discovered service");
cleanup:
    g_clear_object(&serviceProxy);
    return found;
}

static bool TryProcessAsCharacteristic
(
    GDBusObject *object,
    const gchar *objectPath,
    const gchar *devicePath
)
{
    BluezGattCharacteristic1 *characteristicProxy = BLUEZ_GATT_CHARACTERISTIC1(
        g_dbus_object_get_interface(object, "org.bluez.GattCharacteristic1"));
    if (characteristicProxy == NULL)
    {
        return false;
    }
    const gchar *characteristicUuid = bluez_gatt_characteristic1_get_uuid(characteristicProxy);

    if (IsCharacteristicInService(IRTemperatureSensorServicePath, characteristicProxy))
    {
        if (IRTemperatureCharacteristicData == NULL &&
            g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_IR_TEMP_DATA) == 0)
        {
            LE_DEBUG("IR temperature data characteristic found at: %s", objectPath);
            IRTemperatureCharacteristicData = characteristicProxy;
            return true;
        }
        if (IRTemperatureCharacteristicConfig == NULL &&
                 g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_IR_TEMP_CONFIG) == 0)
        {
            LE_DEBUG("IR temperature config characteristic found at: %s", objectPath);
            IRTemperatureCharacteristicConfig = characteristicProxy;
            return true;
        }
        if (IRTemperatureCharacteristicPeriod == NULL &&
                 g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_IR_TEMP_PERIOD) == 0)
        {
            LE_DEBUG("IR temperature period characteristic found at: %s", objectPath);
            IRTemperatureCharacteristicPeriod = characteristicProxy;
            return true;
        }
    }
    else if (IsCharacteristicInService(HumiditySensorServicePath, characteristicProxy))
    {
        if (HumidityCharacteristicData == NULL &&
            g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_HUMIDITY_DATA) == 0)
        {
            LE_DEBUG("Humidity data characteristic found at: %s", objectPath);
            HumidityCharacteristicData = characteristicProxy;
            return true;
        }
        if (HumidityCharacteristicConfig == NULL &&
                 g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_HUMIDITY_CONFIG) == 0)
        {
            LE_DEBUG("Humidity config characteristic found at: %s", objectPath);
            HumidityCharacteristicConfig = characteristicProxy;
            return true;
        }
        if (HumidityCharacteristicPeriod == NULL &&
                 g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_HUMIDITY_PERIOD) == 0)
        {
            LE_DEBUG("Humidity period characteristic found at: %s", objectPath);
            HumidityCharacteristicPeriod = characteristicProxy;
            return true;
        }
    }
    else if (IsCharacteristicInService(IOServicePath, characteristicProxy))
    {
        if (IOCharacteristicData == NULL &&
            g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_IO_DATA) == 0)
        {
            LE_DEBUG("IO data characteristic found at: %s", objectPath);
            IOCharacteristicData = characteristicProxy;
            return true;
        }
        if (IOCharacteristicConfig == NULL &&
                 g_strcmp0(characteristicUuid, CHARACTERISTIC_UUID_IO_CONFIG) == 0)
        {
            LE_DEBUG("IO temperature config characteristic found at: %s", objectPath);
            IOCharacteristicConfig = characteristicProxy;
            return true;
        }
    }

    LE_DEBUG("Ignoring discovered characteristic");
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

    if (
        (
            TryProcessAsService(object, unknownObjectPath, devicePath) ||
            TryProcessAsCharacteristic(object, unknownObjectPath, devicePath)
        ) &&
        IRTemperatureSensorServicePath &&
        IRTemperatureCharacteristicData &&
        IRTemperatureCharacteristicConfig &&
        IRTemperatureCharacteristicPeriod &&
        HumiditySensorServicePath &&
        HumidityCharacteristicData &&
        HumidityCharacteristicConfig &&
        HumidityCharacteristicPeriod &&
        IOServicePath &&
        IOCharacteristicData &&
        IOCharacteristicConfig)
    {
        LE_DEBUG("Found all attributes");
        AllAttributesFoundHandler();
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
         node != NULL && SensorTagDeviceInterface == NULL;
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
            ValidateIrTempPeriod(DHubState.irTemp.period) == PERIOD_VALIDITY_OK)
        {
            IrTemperatureSetEnable(DHubState.irTemp.enable);
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
    switch (ValidateIrTempPeriod(period))
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
                IrTemperatureSetPeriod(period);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.irTemp.enable)
                {
                    const bool oldPeriodValid =
                        ValidateIrTempPeriod(DHubState.irTemp.period) == PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        IrTemperatureSetEnable(true);
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
            ValidateHumidityPeriod(DHubState.humidity.period) == PERIOD_VALIDITY_OK)
        {
            HumiditySetEnable(DHubState.humidity.enable);
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
    switch (ValidateHumidityPeriod(period))
    {
    case PERIOD_VALIDITY_TOO_SHORT:
        LE_WARN(
            "Not setting Humidity sensor period to %lf: minimum period is %lf",
            period,
            IR_TEMP_PERIOD_MIN);
        break;

    case PERIOD_VALIDITY_TOO_LONG:
        LE_WARN(
            "Not setting Humidity sensor period to %lf: maximum period is %lf",
            period,
            IR_TEMP_PERIOD_MAX);
        break;

    case PERIOD_VALIDITY_OK:
        if (period != DHubState.humidity.period)
        {
            if (AppState == APP_STATE_SAMPLING)
            {
                HumiditySetPeriod(period);
                /*
                 * If the enable is already true, but the old period was invalid (because it had
                 * never been set before), then we need to perform an enable.
                 */
                if (DHubState.humidity.enable)
                {
                    const bool oldPeriodValid =
                        ValidateHumidityPeriod(DHubState.humidity.period) == PERIOD_VALIDITY_OK;
                    if (!oldPeriodValid)
                    {
                        HumiditySetEnable(true);
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

    // IO
    LE_ASSERT_OK(io_CreateOutput(RES_PATH_IO_VALUE, IO_DATA_TYPE_JSON, ""));
    io_AddJsonPushHandler(RES_PATH_IO_VALUE, HandleIOPush, NULL);
    io_SetJsonDefault(
        RES_PATH_IO_VALUE, "{ \"redLed\": false, \"greenLed\": false, \"buzzer\": false }");

    le_event_QueueFunction(GlibInit, NULL, NULL);
}
