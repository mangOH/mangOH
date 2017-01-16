/**
 * @file
 *
 * This sample app is a blinking LED at 1 second interval
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"

#define LED_SAMPLE_INTERVAL_IN_MILLISECONDS (1000)

static int8_t state = 0;

//--------------------------------------------------------------------------------------------------
/**
 * Sets default configuration LED D750 as on
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureLed
(
    void
)
{
    // Configure initial value as LED off
    LE_FATAL_IF(mangoh_ledGpio_SetPushPullOutput(MANGOH_LEDGPIO_ACTIVE_HIGH, true) != LE_OK,
                "Couldn't configure LED PLAY as a push pull output");
}

//--------------------------------------------------------------------------------------------------
/**
 * LED D750 changes state as defined by LED_SAMPLE_INTERVAL_IN_SECONDS
 */
//--------------------------------------------------------------------------------------------------
static void ledTimer
(
    le_timer_Ref_t ledTimerRef
)
{
    mangoh_ledGpio_SetEdgeSense(MANGOH_LEDGPIO_EDGE_BOTH);
    if (state)
    {
        mangoh_ledGpio_Activate();
        state = false;
    }
    else
    {
        mangoh_ledGpio_Deactivate();
        state = true;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Main program starts here
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("===============blinking LED application has started");

    ConfigureLed();

    le_timer_Ref_t ledTimerRef = le_timer_Create("LED Timer");
    le_timer_SetMsInterval(ledTimerRef, LED_SAMPLE_INTERVAL_IN_MILLISECONDS);
    le_timer_SetRepeat(ledTimerRef, 0);

    le_timer_SetHandler(ledTimerRef,ledTimer);
    le_timer_Start(ledTimerRef);
}