/**
 *
 * This file provides the implementation of @ref c_led
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"

static const char LedFilename[] = "/sys/devices/platform/led.0/led";

//--------------------------------------------------------------------------------------------------
/**
 * Turn on/off the LED
 */
//--------------------------------------------------------------------------------------------------
static void LedWrite(bool on)
{
    const char *writeData = on ? "1" : "0";
    FILE *ledFile = fopen(LedFilename, "r+");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", LedFilename, errno);
        return;
    }

    LE_DEBUG("Turn %s LED", on ? "on" : "off");
    if (fwrite(writeData, sizeof(writeData), 1, ledFile) != 1)
    {
        LE_ERROR("Write LED device file('%s') failed", LedFilename);
    }

    fclose(ledFile);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn ON the LED
 */
//--------------------------------------------------------------------------------------------------
void ma_led_TurnOn(void)
{
    const bool on = true;
    LedWrite(on);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn OFF the LED
 */
//--------------------------------------------------------------------------------------------------
void ma_led_TurnOff(void)
{
    const bool on = false;
    LedWrite(on);
}

//--------------------------------------------------------------------------------------------------
/**
 * Get the LED status
 *
 * @return
 *	- OFF
 *  - ON
 */
//--------------------------------------------------------------------------------------------------
ma_led_LedStatus_t ma_led_GetLedStatus(void)
{
    uint8_t buf[2];
    FILE *ledFile = NULL;
    ma_led_LedStatus_t res = MA_LED_UNKNOWN;

    ledFile = fopen(LedFilename, "r");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", LedFilename, errno);
        goto done;
    }

    LE_DEBUG("Read LED state");
    if (fread(buf, sizeof(buf), 1, ledFile) != 1)
    {
        LE_ERROR("Read LED device file('%s') failed", LedFilename);
    }
    else if (buf[0] == '0')
    {
        res = MA_LED_OFF;
    }
    else if (buf[0] == '1')
    {
        res = MA_LED_ON;
    }
    fclose(ledFile);

done:
    return res;
}

COMPONENT_INIT
{
    LE_DEBUG("LED Service started");
}
