#include "legato.h"
#include "interfaces.h"
#include "bme680_linux_i2c.h"

#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>


/*
 * Private data that is opaque to clients.
 */
struct bme680_linux_priv
{
    int i2c_bus_fd;
    uint16_t i2c_bus_num;
    uint16_t i2c_addr;
};

static void bme680_linux_delay_ms(uint32_t period_ms, void *context)
{
    const struct timespec delay = {
        .tv_sec = period_ms / 1000,
        .tv_nsec = (period_ms % 1000) * 1000 * 1000,
    };
    int ret = nanosleep(&delay, NULL);
    if (ret) {
        LE_ERROR("nanosleep failed with error: %s\n", strerror(errno));
    }
}

static int8_t bme680_linux_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint16_t len, void *context)
{
    int8_t res = 0; /* Return 0 for Success, non-zero for failure */
    struct bme680_linux *bme680 = context;

    struct i2c_msg i2c_msgs[] = {
        {
            .addr = bme680->priv->i2c_addr,
            .flags = 0,
            .buf = &reg_addr,
            .len = 1,
        },
        {
            .addr = bme680->priv->i2c_addr,
            .flags = I2C_M_RD,
            .buf = reg_data,
            .len = len,
        },
    };
    struct i2c_rdwr_ioctl_data i2c_xfer = {
        .msgs = i2c_msgs,
        .nmsgs = NUM_ARRAY_MEMBERS(i2c_msgs),
    };
    if (ioctl(bme680->priv->i2c_bus_fd, I2C_RDWR, &i2c_xfer) < 0) {
        LE_WARN(
            "I2C read on bus %u, address %u to register %u of length %u failed - %s\n",
            bme680->priv->i2c_bus_num, bme680->priv->i2c_addr, reg_addr, len, strerror(errno));
        res = BME680_E_COM_FAIL;
    }

    return res;
}

static int8_t bme680_linux_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint16_t len, void *context)
{
    uint8_t buffer[64];
    int8_t res = 0; /* Return 0 for Success, non-zero for failure */
    struct bme680_linux *bme680 = context;

    if (len >= sizeof(buffer)) {
        LE_ERROR("I2C write buffer is too small (%u)\n", sizeof(buffer));
        return BME680_E_INVALID_LENGTH;
    }
    buffer[0] = reg_addr;
    memcpy(&buffer[1], reg_data, len);

    struct i2c_msg i2c_msgs[] = {
        {
            .addr = bme680->priv->i2c_addr,
            .flags = 0,
            .buf = buffer,
            .len = len + 1,
        },
    };
    struct i2c_rdwr_ioctl_data i2c_xfer = {
        .msgs = i2c_msgs,
        .nmsgs = NUM_ARRAY_MEMBERS(i2c_msgs),
    };
    if (ioctl(bme680->priv->i2c_bus_fd, I2C_RDWR, &i2c_xfer) < 0) {
        LE_WARN(
            "I2C write on bus %u, address %u to register %u of length %u failed - %s\n",
            bme680->priv->i2c_bus_num, bme680->priv->i2c_addr, reg_addr, len, strerror(errno));
        res = BME680_E_COM_FAIL;
    }

    return res;
}

struct bme680_linux* bme680_linux_i2c_create(
    unsigned i2c_bus_num, uint8_t i2c_addr, bme680_ambient_temperature_fptr_t read_ambient_temperature)
{
    struct bme680_linux *bme680 = calloc(sizeof(*bme680), 1);
    if (!bme680)
        return NULL;
    bme680->priv = calloc(sizeof(*(bme680->priv)), 1);
    if (!bme680->priv)
        goto fail;

    // Initialize Bosch driver struct
    bme680->dev.context = bme680;
    bme680->dev.intf = BME680_I2C_INTF;
    bme680->dev.read = bme680_linux_i2c_read;
    bme680->dev.write = bme680_linux_i2c_write;
    bme680->dev.delay_ms = bme680_linux_delay_ms;
    bme680->dev.read_ambient_temperature = read_ambient_temperature;

    // Initialize private data required for read/write/delay_ms
    bme680->priv->i2c_bus_num = i2c_bus_num;
    bme680->priv->i2c_addr = i2c_addr;
    char bus_filename[100];
    int needed_len = snprintf(bus_filename, sizeof(bus_filename) - 1, "/dev/i2c-%u", i2c_bus_num);
    LE_ASSERT(needed_len < sizeof(bus_filename));
    bme680->priv->i2c_bus_fd = open(bus_filename, O_RDWR);
    if (bme680->priv->i2c_bus_fd < 0) {
        LE_ERROR("Couldn't open I2C bus %u - %s\n", i2c_bus_num, strerror(errno));
        goto fail;
    }

    return bme680;

fail:
    if (bme680->priv)
        free(bme680->priv);
    free(bme680);
    return NULL;
}

void bme680_linux_i2c_destroy(struct bme680_linux* bme680)
{
    int close_res = close(bme680->priv->i2c_bus_fd);
    if (close_res != 0) {
        LE_ERROR("Couldn't close I2C bus file descriptor - %s\n", strerror(errno));
    }
    free(bme680->priv);
    free(bme680);
}

COMPONENT_INIT
{
}
