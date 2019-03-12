#ifndef _BME680_LINUX_I2C_H_
#define _BME680_LINUX_I2C_H_

#include "legato.h"
#include "bme680_defs.h"
#include <stdint.h>

typedef struct bme680_linux_priv bme680_linux_priv;

struct bme680_linux
{
    struct bme680_dev dev;
    bme680_linux_priv* priv;
};

struct bme680_linux* bme680_linux_i2c_create(
    unsigned i2c_bus_num, uint8_t i2c_addr, bme680_ambient_temperature_fptr_t read_ambient_temperature);
void bme680_linux_i2c_destroy(struct bme680_linux* bme680);

#endif // _BME680_LINUX_I2C_H_
