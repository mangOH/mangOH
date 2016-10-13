//--------------------------------------------------------------------------------------------------
/** 
 * This app reads the current adc reading every 1 seconds 
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * The time between publishing ADC location readings
 *
 * @note Please change this timeout value as needed.
 */
//--------------------------------------------------------------------------------------------------
#define ADC_SAMPLE_INTERVAL_IN_MILLI_SECONDS (1)

//--------------------------------------------------------------------------------------------------
/**
 * Timer handler  will publish the current ADC reading.
 */
//--------------------------------------------------------------------------------------------------
static void adcTimer
(
    le_timer_Ref_t adcTimerRef
)
{
    int32_t value;

    const le_result_t result = le_adc_ReadValue("EXT_ADC1", &value);

    if (result == LE_OK)
    {
        LE_INFO("EXT_ADC1 value is: %d", value);
    }
    else
    {
        LE_INFO("Couldn't get ADC value");
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Main program starts here
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("---------------------- ADC Reading started");

    le_timer_Ref_t adcTimerRef = le_timer_Create("ADC Timer");
    le_timer_SetMsInterval(adcTimerRef, ADC_SAMPLE_INTERVAL_IN_MILLI_SECONDS * 1000);
    le_timer_SetRepeat(adcTimerRef, 0);

    le_timer_SetHandler(adcTimerRef, adcTimer);

    le_timer_Start(adcTimerRef);
}
