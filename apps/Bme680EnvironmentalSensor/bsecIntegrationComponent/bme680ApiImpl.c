#include "legato.h"
#include "interfaces.h"
#include "bsec_interface.h"
#include "bsecIntegration.h"

extern struct Bme680State _s;

le_result_t mangOH_bme680_Configure(
    mangOH_bme680_SamplingRate_t samplingRate,
    bool enableIaq,
    bool enableCo2Equivalent,
    bool enableBreathVoc,
    bool enablePressure,
    bool enableTemperature,
    bool enableHumidity)
{

    float sr;
    switch (samplingRate)
    {
    case MANGOH_BME680_SAMPLING_RATE_DISABLED:
        sr = BSEC_SAMPLE_RATE_DISABLED;
        break;
    case MANGOH_BME680_SAMPLING_RATE_LP:
        sr = BSEC_SAMPLE_RATE_LP;
        break;
    case MANGOH_BME680_SAMPLING_RATE_ULP:
        sr = BSEC_SAMPLE_RATE_ULP;
        break;
    default:
        return LE_BAD_PARAMETER;
    }

    _s.enableIaq = enableIaq;
    _s.enableCo2Equivalent = enableCo2Equivalent;
    _s.enableBreathVoc = enableBreathVoc;
    _s.enablePressure = enablePressure;
    _s.enableTemperature = enableTemperature;
    _s.enableHumidity = enableHumidity;

    bsec_sensor_configuration_t virtualOutputs[] = {
        {
            .sensor_id = BSEC_OUTPUT_IAQ,
            .sample_rate = enableIaq ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
        {
            .sensor_id = BSEC_OUTPUT_CO2_EQUIVALENT,
            .sample_rate = enableCo2Equivalent ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
        {
            .sensor_id = BSEC_OUTPUT_BREATH_VOC_EQUIVALENT,
            .sample_rate = enableBreathVoc ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
        {
            .sensor_id = BSEC_OUTPUT_RAW_PRESSURE,
            .sample_rate = enablePressure ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
        {
            .sensor_id = BSEC_OUTPUT_RAW_TEMPERATURE,
            .sample_rate = sr,
        },
        {
            .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
            .sample_rate = enableTemperature ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
        {
            .sensor_id = BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
            .sample_rate = enableHumidity ? sr : BSEC_SAMPLE_RATE_DISABLED,
        },
    };

    bsec_sensor_configuration_t requiredSensorSettings[BSEC_MAX_PHYSICAL_SENSOR];
    uint8_t numRequiredSensorSettings = BSEC_MAX_PHYSICAL_SENSOR;
    bsec_library_return_t status = bsec_update_subscription(
        virtualOutputs,
        NUM_ARRAY_MEMBERS(virtualOutputs),
        requiredSensorSettings,
        &numRequiredSensorSettings);

    if (samplingRate != _s.samplingRate)
    {
        _s.samplingRate = samplingRate;
        LE_ASSERT_OK(le_timer_SetMsInterval(_s.timer, (uint32_t)(1000.0 / sr)));
        if (!le_timer_IsRunning(_s.timer))
        {
            LE_ASSERT_OK(le_timer_Start(_s.timer));
        }
    }

    return status == BSEC_OK ? LE_OK : LE_FAULT;
}

mangOH_bme680_SensorReadingHandlerRef_t mangOH_bme680_AddSensorReadingHandler
(
    mangOH_bme680_SensorReadingHandlerFunc_t handlerFunc,
    void* contextPtr
)
{
    struct Bme680HandlerMapping *hm = le_mem_ForceAlloc(_s.HandlerPool);
    hm->owningSession = mangOH_bme680_GetClientSessionRef();
    hm->handlerFunc = handlerFunc;
    hm->context = contextPtr;

    return le_ref_CreateRef(_s.HandlerRefMap, hm);
}

void mangOH_bme680_RemoveSensorReadingHandler
(
    mangOH_bme680_SensorReadingHandlerRef_t handlerRef
)
{
    struct Bme680HandlerMapping *hm = le_ref_Lookup(_s.HandlerRefMap, handlerRef);
    if (!hm)
    {
        LE_WARN("No handler found for the given reference");
        return;
    }

    if (mangOH_bme680_GetClientSessionRef() != hm->owningSession)
    {
        LE_WARN("Cannot delete handler that isn't owned");
        return;
    }

    le_ref_DeleteRef(_s.HandlerRefMap, handlerRef);
    le_mem_Release(hm);
}
