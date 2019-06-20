#include "legato.h"
#include "interfaces.h"
#include "bsec_interface.h"
#include "bme680_linux_i2c.h"
#include "bme680.h"
#include "bsecIntegration.h"

#define BSEC_STATE_FILE "bsec_state.bin"
#define SAMPLES_BETWEEN_SAVES 200
#define BME680_I2C_ADDR 0x76

struct Bme680State _s;

static bool AmbientTempApiConnected = false;

int64_t GetTimestampNs(void)
{
    struct timespec monotime;
    if (clock_gettime(CLOCK_MONOTONIC, &monotime)) {
        LE_ERROR("couldn't get timestamp - %s\n", strerror(errno));
    }
    return (monotime.tv_sec * 1000LL * 1000LL * 1000LL) + (monotime.tv_nsec);
}

static void SleepNs(uint64_t ns)
{
    const uint64_t ns_per_s = (1000LL * 1000LL * 1000LL);
    const struct timespec delay = {
        .tv_sec = ns / ns_per_s,
        .tv_nsec = ns % ns_per_s,
    };
    int ret = nanosleep(&delay, NULL);
    if (ret) {
        LE_ERROR("nanosleep failed with error: %s\n", strerror(errno));
    }
}

/*
static le_result_t LoadState(void)
{
    le_result_t res = LE_OK;
    uint8_t bsecState[BSEC_MAX_PROPERTY_BLOB_SIZE] = {0};

    int fd = open(BSEC_STATE_FILE, O_RDONLY);
    if (fd == -1)
    {
        LE_ERROR("Couldn't open \"%s\" for reading - %s\n", BSEC_STATE_FILE, strerror(errno));
        res = LE_FAULT;
        goto done;
    }

    size_t numRead = 0;
    while (true) {
        ssize_t spaceRemaining = NUM_ARRAY_MEMBERS(bsecState) - numRead;
        if (spaceRemaining == 0) {
            LE_ERROR("File content in \"%s\" too large to fit in buffer\n", BSEC_STATE_FILE);
            res = LE_FAULT;
            goto fail_read;
        }
        int readRes = read(fd, &bsecState[numRead], spaceRemaining);
        if (readRes < 0) {
            LE_ERROR("Failed to read \"%s\" - %s\n", BSEC_STATE_FILE, strerror(errno));
            res = LE_FAULT;
            goto fail_read;
        }
        if (readRes == 0) {
            // Reached EOF
            break;
        }
        numRead += readRes;
    }
    close(fd);

    uint8_t workBuffer[BSEC_MAX_PROPERTY_BLOB_SIZE] = {0};
    bsec_library_return_t bsecRes = bsec_set_state(bsecState, numRead, workBuffer, NUM_ARRAY_MEMBERS(workBuffer));
    if (bsecRes != BSEC_OK)
    {
        LE_ERROR("Failed to set BSEC state");
        res = LE_FAULT;
    }

    return res;

fail_read:
    close(fd);
done:
    return res;
}
*/

static le_result_t SaveState(void)
{
    le_result_t ret = LE_OK;

    uint8_t bsecState[BSEC_MAX_STATE_BLOB_SIZE];
    uint8_t workBuffer[BSEC_MAX_STATE_BLOB_SIZE];
    uint32_t bsecStateLen = 0;
    bsec_library_return_t bsecStatus = bsec_get_state(
        0, bsecState, sizeof(bsecState), workBuffer, sizeof(workBuffer), &bsecStateLen);
    if (bsecStatus != BSEC_OK)
    {
        LE_ERROR("Couldn't get state out of BSEC");
        ret = LE_FAULT;
        goto done;
    }

    int fd = open(BSEC_STATE_FILE, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        LE_ERROR("Couldn't open \"%s\" for writing - %s\n", BSEC_STATE_FILE, strerror(errno));
        ret = LE_FAULT;
        goto done;
    }

    unsigned int numWritten = 0;
    do {
        const size_t numToWrite = bsecStateLen - numWritten;
        int res = write(fd, &bsecState[numWritten], numToWrite);
        if (res < 0) {
            LE_ERROR("Failed to write \"%s\" - %s\n", BSEC_STATE_FILE, strerror(errno));
            ret = LE_FAULT;
            goto done;
        }
        numWritten += res;
    } while (numWritten < bsecStateLen);

    int res = fsync(fd);
    if (res) {
        LE_ERROR("Failed to write \"%s\" - %s\n", BSEC_STATE_FILE, strerror(errno));
        ret = LE_FAULT;
        goto done;
    }

done:
    close(fd);
    return ret;
}

static void TriggerMeasurement(struct bme680_dev *bme680, const bsec_bme_settings_t *sensorSettings)
{
    if (sensorSettings->trigger_measurement)
    {
        bme680->tph_sett.os_hum = sensorSettings->humidity_oversampling;
        bme680->tph_sett.os_pres = sensorSettings->pressure_oversampling;
        bme680->tph_sett.os_temp = sensorSettings->temperature_oversampling;
        bme680->gas_sett.run_gas = sensorSettings->run_gas;
        bme680->gas_sett.heatr_temp = sensorSettings->heater_temperature;
        bme680->gas_sett.heatr_dur = sensorSettings->heating_duration;
        bme680->power_mode = BME680_FORCED_MODE;

        int8_t bme680Status = bme680_set_sensor_settings(
            BME680_OST_SEL | BME680_OSP_SEL | BME680_OSH_SEL | BME680_GAS_SENSOR_SEL, bme680);
        if (bme680Status != BME680_OK)
        {
            LE_ERROR("Failed to set sensor settings");
            return;
        }
        bme680Status = bme680_set_sensor_mode(bme680);
        if (bme680Status != BME680_OK)
        {
            LE_ERROR("Failed to set sensor mode");
            return;
        }

        uint16_t measPeriodMs;
        bme680_get_profile_dur(&measPeriodMs, bme680);

        SleepNs(measPeriodMs * 1000 * 1000);
    }

    do
    {
        int8_t bme680Status = bme680_get_sensor_mode(bme680);
        if (bme680Status == BME680_OK && bme680->power_mode != BME680_FORCED_MODE)
        {
            break;
        }
        SleepNs(5 * 1000 * 1000);
    } while(true);
}

static void ReadData(
    struct bme680_dev *bme680,
    int64_t timestampTrigger,
    bsec_input_t *inputs,
    size_t *numBsecInputs,
    const bsec_bme_settings_t *sensorSettings)
{
    if (sensorSettings->process_data)
    {
        struct bme680_field_data data;
        int8_t bme680Status = bme680_get_sensor_data(&data, bme680);
        if (bme680Status != BME680_OK)
        {
            LE_ERROR("Failed to read data from the bme680");
            return;
        }

        if (data.status & BME680_NEW_DATA_MSK)
        {
            /* Pressure to be processed by BSEC */
            if (sensorSettings->process_data & BSEC_PROCESS_PRESSURE)
            {
                /* Place presssure sample into input struct */
                inputs[*numBsecInputs].sensor_id = BSEC_INPUT_PRESSURE;
                inputs[*numBsecInputs].signal = data.pressure;
                inputs[*numBsecInputs].time_stamp = timestampTrigger;
                (*numBsecInputs)++;
            }
            /* Temperature to be processed by BSEC */
            if (sensorSettings->process_data & BSEC_PROCESS_TEMPERATURE)
            {
                /* Place temperature sample into input struct */
                inputs[*numBsecInputs].sensor_id = BSEC_INPUT_TEMPERATURE;
                #ifdef BME680_FLOAT_POINT_COMPENSATION
                    inputs[*numBsecInputs].signal = data.temperature;
                #else
                    inputs[*numBsecInputs].signal = data.temperature / 100.0f;
                #endif
                inputs[*numBsecInputs].time_stamp = timestampTrigger;
                (*numBsecInputs)++;

                /*
                 * Also add optional heatsource input which will be subtracted from the temperature
                 * reading to compensate for device-specific self-heating (supported in BSEC IAQ
                 * solution)
                 */
                inputs[*numBsecInputs].sensor_id = BSEC_INPUT_HEATSOURCE;
                inputs[*numBsecInputs].signal = _s.temperatureOffset;
                inputs[*numBsecInputs].time_stamp = timestampTrigger;
                (*numBsecInputs)++;
            }
            /* Humidity to be processed by BSEC */
            if (sensorSettings->process_data & BSEC_PROCESS_HUMIDITY)
            {
                /* Place humidity sample into input struct */
                inputs[*numBsecInputs].sensor_id = BSEC_INPUT_HUMIDITY;
                #ifdef BME680_FLOAT_POINT_COMPENSATION
                    inputs[*numBsecInputs].signal = data.humidity;
                #else
                    inputs[*numBsecInputs].signal = data.humidity / 1000.0f;
                #endif
                inputs[*numBsecInputs].time_stamp = timestampTrigger;
                (*numBsecInputs)++;
            }
            /* Gas to be processed by BSEC */
            if (sensorSettings->process_data & BSEC_PROCESS_GAS)
            {
                /* Check whether gas_valid flag is set */
                if(data.status & BME680_GASM_VALID_MSK)
                {
                    /* Place sample into input struct */
                    inputs[*numBsecInputs].sensor_id = BSEC_INPUT_GASRESISTOR;
                    inputs[*numBsecInputs].signal = data.gas_resistance;
                    inputs[*numBsecInputs].time_stamp = timestampTrigger;
                    (*numBsecInputs)++;
                }
            }
        }
    }
}

static void ProcessData(bsec_input_t *bsecInputs, size_t numBsecInputs, int64_t timestamp)
{
    if (numBsecInputs > 0)
    {
        mangOH_bme680_Reading_t reading = {.timestamp = timestamp};
        bsec_output_t bsecOutputs[BSEC_NUMBER_OUTPUTS];
        uint8_t numBsecOutputs = NUM_ARRAY_MEMBERS(bsecOutputs);
        bsec_library_return_t bsecStatus = bsec_do_steps(bsecInputs, numBsecInputs, bsecOutputs, &numBsecOutputs);
        LE_ASSERT(bsecStatus == BSEC_OK);
        for (size_t i = 0; i < numBsecOutputs; i++)
        {
            switch (bsecOutputs[i].sensor_id)
            {
            case BSEC_OUTPUT_IAQ:
                reading.iaq.valid = true;
                reading.iaq.value = bsecOutputs[i].signal;
                reading.iaq.accuracy = bsecOutputs[i].accuracy;
                break;
            case BSEC_OUTPUT_CO2_EQUIVALENT:
                reading.co2Equivalent.valid = true;
                reading.co2Equivalent.value = bsecOutputs[i].signal;
                reading.co2Equivalent.accuracy = bsecOutputs[i].accuracy;
                break;
            case BSEC_OUTPUT_BREATH_VOC_EQUIVALENT:
                reading.breathVoc.valid = true;
                reading.breathVoc.value = bsecOutputs[i].signal;
                reading.breathVoc.accuracy = bsecOutputs[i].accuracy;
                break;
            case BSEC_OUTPUT_RAW_TEMPERATURE:
                /*
                 * Update the temperature offset based upon the raw temperature from the BME680 and
                 * the ambient temperature given by the ambient temperature API. It is assumed that
                 * the value from the ambient temperature is the true ambient temperature.
                 */
                if (!AmbientTempApiConnected)
                {
                    le_result_t result = ambient_TryConnectService();
                    LE_DEBUG("ambient_TryConnect() returned %s", LE_RESULT_TXT(result));
                    if (result == LE_OK)
                    {
                        AmbientTempApiConnected = true;
                    }
                }
                if (AmbientTempApiConnected)
                {
                    double ambientTemperature;
                    le_result_t ambientRes = ambient_Read(&ambientTemperature);
                    if (ambientRes == LE_OK)
                    {
                        double delta = bsecOutputs[i].signal - ambientTemperature;
                        _s.temperatureOffset = (delta * 0.25) + (_s.temperatureOffset * 0.75);
                        LE_INFO("Temperature offset is now %f", _s.temperatureOffset);
                    }
                }
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE:
                reading.temperature.valid = true;
                reading.temperature.value = bsecOutputs[i].signal;
                break;
            case BSEC_OUTPUT_RAW_PRESSURE:
                reading.pressure.valid = true;
                reading.pressure.value = bsecOutputs[i].signal;
                break;
            case BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY:
                reading.humidity.valid = true;
                reading.humidity.value = bsecOutputs[i].signal;
                break;
            default:
                LE_ERROR("unhandled data output type (%d)", bsecOutputs[i].sensor_id);
                break;
            }
        }

        le_ref_IterRef_t it = le_ref_GetIterator(_s.HandlerRefMap);
        while (le_ref_NextNode(it) == LE_OK)
        {
            struct Bme680HandlerMapping *hm = le_ref_GetValue(it);
            hm->handlerFunc(&reading, hm->context);
        }
    }
}

static void TimerHandler(le_timer_Ref_t t)
{
    static size_t samplesSinceSave = 0;
    bsec_bme_settings_t sensorSettings;
    int64_t timestamp = GetTimestampNs();
    bsec_sensor_control(timestamp, &sensorSettings);

    TriggerMeasurement(&_s.bme680->dev, &sensorSettings);

    /* Allocate enough memory for up to BSEC_MAX_PHYSICAL_SENSOR physical inputs*/
    bsec_input_t bsecInputs[BSEC_MAX_PHYSICAL_SENSOR];

    /* Number of inputs to BSEC */
    size_t numBsecInputs = 0;
    ReadData(&_s.bme680->dev, timestamp, bsecInputs, &numBsecInputs, &sensorSettings);

    ProcessData(bsecInputs, numBsecInputs, timestamp);

    samplesSinceSave++;
    if (samplesSinceSave >= SAMPLES_BETWEEN_SAVES)
    {
        if (SaveState() == LE_OK)
        {
            samplesSinceSave = 0;
        }
    }
    int64_t nextTimerMs = (sensorSettings.next_call - GetTimestampNs()) / (1000LL * 1000LL);
    LE_DEBUG("Configuring timer to %" PRId64 " ms", nextTimerMs);
    if (nextTimerMs <= 0)
    {
        LE_WARN("Next timer set to occur in %" PRId64 " ms. Setting to 1 ms instead", nextTimerMs);
        nextTimerMs = 1;
    }
    LE_ASSERT_OK(le_timer_SetMsInterval(t, nextTimerMs));
    LE_ASSERT_OK(le_timer_Start(t));
}

static void ClientSessionClosedHandler
(
    le_msg_SessionRef_t clientSession,
    void *context
)
{
    le_ref_IterRef_t it = le_ref_GetIterator(_s.HandlerRefMap);

    bool finished = le_ref_NextNode(it) != LE_OK;
    while (!finished)
    {
        struct Bme680HandlerMapping *hm = le_ref_GetValue(it);
        LE_ASSERT(hm != NULL);
        // In order to prevent invalidating the iterator, we store the reference of the device we
        // want to close and advance the iterator before calling le_spi_Close which will remove the
        // reference from the hashmap.
        void* refToRemove =
            (hm->owningSession == clientSession) ? ((void*)le_ref_GetSafeRef(it)) : NULL;
        finished = le_ref_NextNode(it) != LE_OK;
        if (refToRemove != NULL)
        {
            le_ref_DeleteRef(_s.HandlerRefMap, refToRemove);
            le_mem_Release(hm);
        }
    }
}

int8_t ReadAmbientTemperature(void)
{
    int8_t ambientInt = 20;
    double ambientTemperature;
    le_result_t ambientRes = ambient_Read(&ambientTemperature);
    if (ambientRes == LE_OK)
    {
        ambientInt = (int8_t)round(ambientTemperature);
    }
    else
    {
        LE_ERROR("Retrieval of ambient temperture failed");
    }

    return ambientInt;
}

COMPONENT_INIT
{
    bsec_library_return_t bsecRes;
    bsec_version_t bsecVersion;

    bsecRes = bsec_init();
    LE_FATAL_IF(bsecRes != BSEC_OK, "Failed to initialize the BSEC library. Error: %d", bsecRes);

    bsecRes = bsec_get_version(&bsecVersion);
    LE_FATAL_IF(bsecRes != BSEC_OK, "Failed to lookup BSEC library version. Error: %d", bsecRes);
    LE_INFO(
        "Using BSEC library v%d.%d.%d.%d",
        bsecVersion.major,
        bsecVersion.minor,
        bsecVersion.major_bugfix,
        bsecVersion.minor_bugfix);

    /*
     * It seems that sometimes the state is going "bad" in some way and this can
     * lead to abnormally high humidity readings (for example). Disable loading
     * of the state for now so that we can ensure a clean state on app start.
     */
    // LoadState();

    _s.bme680 = bme680_linux_i2c_create(BME680_I2C_BUS, BME680_I2C_ADDR, ReadAmbientTemperature);
    LE_FATAL_IF(!_s.bme680, "Couldn't create bme680 device");
    if (!_s.bme680) {
        fprintf(stderr, "Failed to create device\n");
        exit(1);
    }

    int8_t initRes = bme680_init(&_s.bme680->dev);
    LE_FATAL_IF(initRes != BME680_OK, "Failed to initialize bme680 library");

    _s.timer = le_timer_Create("bme680 sample");
    LE_ASSERT_OK(le_timer_SetHandler(_s.timer, TimerHandler));
    _s.samplingRate = MANGOH_BME680_SAMPLING_RATE_DISABLED;

    _s.HandlerPool = le_mem_CreatePool("bme680 handlers", sizeof(struct Bme680HandlerMapping));
    _s.HandlerRefMap = le_ref_CreateMap("bme680 refmap", 2);
    le_msg_AddServiceCloseHandler(mangOH_bme680_GetServiceRef(), ClientSessionClosedHandler, NULL);
}
