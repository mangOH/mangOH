#ifndef _BSEC_INTEGRATION_H_
#define _BSEC_INTEGRATION_H_

struct Bme680HandlerMapping
{
    le_msg_SessionRef_t owningSession;
    mangOH_bme680_SensorReadingHandlerFunc_t handlerFunc;
    void *context;
};

struct Bme680State
{
    le_timer_Ref_t timer;
    mangOH_bme680_SamplingRate_t samplingRate;
    bool enableIaq;
    bool enableCo2Equivalent;
    bool enableBreathVoc;
    bool enablePressure;
    bool enableTemperature;
    bool enableHumidity;
    le_mem_PoolRef_t HandlerPool;
    le_ref_MapRef_t HandlerRefMap;
    double ambientTemperature; // Degrees Celcius
    double temperatureOffset;
};

#endif // _BSEC_INTEGRATION_H_
