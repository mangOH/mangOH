#include "legato.h"
#include "interfaces.h"

static void SensorReadingHandler(
    const mangOH_bme680_Reading_t *reading,
    void* context
)
{
    LE_INFO("bme680 reading received with ts=%llu", reading->timestamp);
    LE_INFO("  iaq_valid=%d, iaq=%f, iaq_accuracy=%d",
            reading->iaq.valid, reading->iaq.value, reading->iaq.accuracy);
    LE_INFO("  co2_equivalent_valid=%d, co2_equivalent=%f, co2_equivalent_accuracy=%d",
            reading->co2Equivalent.valid, reading->co2Equivalent.value, reading->co2Equivalent.accuracy);
    LE_INFO("  breath_voc_valid=%d, breath_voc=%f, breath_voc_accuracy=%d",
            reading->breathVoc.valid, reading->breathVoc.value, reading->breathVoc.accuracy);
    LE_INFO("  pressure_valid=%d, pressure=%f", reading->pressure.valid, reading->pressure.value);
    LE_INFO("  temperature_valid=%d, temperature=%f", reading->temperature.valid, reading->temperature.value);
    LE_INFO("  humidity_valid=%d, humidity=%f", reading->humidity.valid, reading->humidity.value);
}

COMPONENT_INIT
{
    mangOH_bme680_AddSensorReadingHandler(SensorReadingHandler, NULL);

    const bool enableIaq = true;
    const bool enableCo2Equivalent = true;
    const bool enableBreathVoc = true;
    const bool enablePressure = true;
    const bool enableTemperature = true;
    const bool enableHumidity = true;
    le_result_t configRes = mangOH_bme680_Configure(
        MANGOH_BME680_SAMPLING_RATE_LP,
        enableIaq,
        enableCo2Equivalent,
        enableBreathVoc,
        enablePressure,
        enableTemperature,
        enableHumidity);
    LE_ASSERT_OK(configRes);
}
