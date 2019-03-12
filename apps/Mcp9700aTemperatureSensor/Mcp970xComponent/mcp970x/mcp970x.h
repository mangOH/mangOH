#ifndef _MCP970X_H_
#define _MCP970X_H_

#include <stdint.h>

/*
 * "A" versions are the same except with better accuracy
 */
enum mcp970x_chip
{
    MCP970X_CHIP_9700,
    MCP970X_CHIP_9700A = MCP970X_CHIP_9700,
    MCP970X_CHIP_9701,
    MCP970X_CHIP_9701A = MCP970X_CHIP_9701,

    MCP970X_CHIP_NUM_OF,
};

typedef int (*mcp970x_adc_function)(int32_t *val_uv);
int mcp970x_read_temperature(
    enum mcp970x_chip chip, mcp970x_adc_function adc, int32_t *temperature);

#endif // _MCP970X_H_
