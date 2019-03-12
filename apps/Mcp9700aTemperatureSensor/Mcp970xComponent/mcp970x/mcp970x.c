#include "mcp970x.h"
#include <errno.h>

struct mcp970x_chip_spec
{
    uint32_t uv_at_zero_celcius;
    // uV/degree celcius
    uint32_t temperature_coefficient;
};

static const struct mcp970x_chip_spec chip_specs[] =
{
    [MCP970X_CHIP_9700] = {
        .uv_at_zero_celcius = 500000,
        .temperature_coefficient = 10000,
    },
    [MCP970X_CHIP_9701] = {
        .uv_at_zero_celcius = 400000,
        .temperature_coefficient = 19500,
    },
};

/*
 * Calculate the temperature in milli-degrees celcius.  Divide by 1000 to get degrees celcius.
 *
 * Datasheet equation 4-1:
 * Vout = (Tc * Ta) + V0c
 * So:
 * Ta = (Vout - V0c) / Tc
 */
static int32_t calculate_temperature(const struct mcp970x_chip_spec *chip_spec, uint32_t voltage_uv)
{
    return (1000L * (voltage_uv - chip_spec->uv_at_zero_celcius)) / chip_spec->temperature_coefficient;
}

int mcp970x_read_temperature(enum mcp970x_chip chip, mcp970x_adc_function adc, int32_t *temperature)
{
    if (chip < 0 || chip >= MCP970X_CHIP_NUM_OF)
    {
        return -EINVAL;
    }

    const struct mcp970x_chip_spec *spec = &chip_specs[chip];
    int32_t adc_uv;
    int res = adc(&adc_uv);
    if (res)
    {
        return res;
    }

    *temperature = calculate_temperature(spec, adc_uv);
    return 0;
}
