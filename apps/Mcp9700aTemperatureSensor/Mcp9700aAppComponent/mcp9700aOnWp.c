#include "legato.h"
#include "interfaces.h"
#include "mcp970x.h"

static int WpAdcFunction(int32_t *valueUv)
{
    return le_adc_ReadValue("EXT_ADC0", valueUv);
}

le_result_t mangOH_ambientTemperature_Read(double *temperature)
{
    int32_t tempInt;
    int res = mcp970x_read_temperature(MCP970X_CHIP_9700A, WpAdcFunction, &tempInt);
    if (res != 0)
    {
        return LE_FAULT;
    }

    *temperature = tempInt / 1000.0;
    return LE_OK;
}


COMPONENT_INIT
{
}
