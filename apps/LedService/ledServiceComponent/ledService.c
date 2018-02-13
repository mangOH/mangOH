/**
 *
 * This file provides the implementation of @ref c_led
 *
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"

//LED file descriptor
static const char ledFileName[] = "/sys/devices/platform/led.0/led";

//--------------------------------------------------------------------------------------------------
/**
 * Turn ON the LED
 */
//--------------------------------------------------------------------------------------------------
void ma_led_TurnOn(void)
{
    FILE *ledFile = NULL;

    ledFile = fopen(ledFileName, "r+");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

    LE_DEBUG("turn on LED");
    if (fwrite("1", strlen("1") + 1, 1, ledFile) <= 0)
    {
        LE_ERROR("Write LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

cleanup:
    if (ledFile) fclose(ledFile);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn OFF the LED
 */
//--------------------------------------------------------------------------------------------------
void ma_led_TurnOff(void)
{
    FILE *ledFile = NULL;

    ledFile = fopen(ledFileName, "r+");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

    LE_DEBUG("turn off LED");
    if (fwrite("0", strlen("0") + 1, 1, ledFile) <= 0)
    {
        LE_ERROR("Write LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

cleanup:
    if (ledFile) fclose(ledFile);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the LED status
 *
 * @return
 *	- OFF
 *      - ON
 */
//--------------------------------------------------------------------------------------------------
ma_led_LedStatus_t ma_led_GetLedStatus(void)
{
    uint8_t buf[2];
    FILE *ledFile = NULL;

    ledFile = fopen(ledFileName, "r+");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

    LE_DEBUG("turn on LED");
    if (fread(buf, sizeof(buf), 1, ledFile) != sizeof(buf)) 
    {
        LE_ERROR("Read LED device file('%s') failed(%d)", ledFileName, errno);
        goto cleanup;
    }

cleanup:
    if (ledFile) fclose(ledFile);
    return (buf[0] == '0') ? MA_LED_OFF:MA_LED_ON;
}

COMPONENT_INIT
{
    LE_INFO("---------------------- LED Service started");
}
