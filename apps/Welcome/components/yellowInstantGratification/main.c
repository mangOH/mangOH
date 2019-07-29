//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the "instant gratification" features for the out-of-box experience.
 * These are things like blinking LEDs and responding to button presses.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

/// Path to the instant gratification enable/disable configuration in the Config Tree.
/// @note There's a bug in le_cfg_AddChangeHandler() that makes it not work unless we specify
///       the tree name explicitly at the beginning of the configTree path.
///       (This was discovered in Legato version 19.04).
static const char ConfigPath[] = "helloYellow:/enableInstantGrat";

/// "Wake-up source" used to keep the device awake when the instant-gratification features
/// are enabled.
le_pm_WakeupSourceRef_t WakeUpSource;


//--------------------------------------------------------------------------------------------------
/**
 * Enable the instant gratification features, like the vegas mode and buzzer on button press.
 */
//--------------------------------------------------------------------------------------------------
static void EnableInstantGratification
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Obtain a wake lock to keep the device awake.
    le_result_t result = le_pm_StayAwake(WakeUpSource);
    LE_CRIT_IF(result != LE_OK, "Failed to grab wake lock (%s)", LE_RESULT_TXT(result));

    // Route button press events to the "vegas mode" trigger and the buzzer enable.
    dhubAdmin_SetSource("/app/vegasMode/triggered/trigger", "/app/button/value");
    dhubAdmin_SetSource("/app/buzzer/enable", "/app/button/value");

    // Set the buzzer to 100% duty cycle, so it stays on as long as the button is held down.
    dhubAdmin_SetNumericOverride("/app/buzzer/percent", 100.0);

    // Route LED states from the vegasMode app to the LED control outputs.
    dhubAdmin_SetSource("/app/leds/mono/enable", "/app/vegasMode/led/0");
    dhubAdmin_SetSource("/app/leds/tri/red/enable", "/app/vegasMode/led/1");
    dhubAdmin_SetSource("/app/leds/tri/green/enable", "/app/vegasMode/led/2");
    dhubAdmin_SetSource("/app/leds/tri/blue/enable", "/app/vegasMode/led/3");

    // Enable continuous (slow) Vegas Mode.
    dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Disable the instant gratification features, like the vegas mode and buzzer on button press.
 */
//--------------------------------------------------------------------------------------------------
static void DisableInstantGratification
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Remove routings from button to "vegas mode" trigger and buzzer enable.
    dhubAdmin_RemoveSource("/app/vegasMode/triggered/trigger");
    dhubAdmin_RemoveSource("/app/buzzer/enable");

    // Remove buzzer duty cycle override.
    dhubAdmin_RemoveOverride("/app/buzzer/percent");

    // Disable continuous (slow) Vegas Mode.
    dhubAdmin_RemoveOverride("/app/vegasMode/continuous/enable");
    dhubAdmin_PushBoolean("/app/vegasMode/continuous/enable", 0, false);

    // Remove routings from vegasMode app to the LED control outputs.
    dhubAdmin_RemoveSource("/app/leds/mono/enable");
    dhubAdmin_RemoveSource("/app/leds/tri/red/enable");
    dhubAdmin_RemoveSource("/app/leds/tri/green/enable");
    dhubAdmin_RemoveSource("/app/leds/tri/blue/enable");

    // Release the wake lock to allow the device to sleep.
    le_result_t result = le_pm_Relax(WakeUpSource);
    LE_CRIT_IF(result != LE_OK, "Failed to release wake lock (%s)", LE_RESULT_TXT(result));
}


//--------------------------------------------------------------------------------------------------
/**
 * Function called-back by the Config Tree when the instant gratification enable/disable
 * configuration changes.
 */
//--------------------------------------------------------------------------------------------------
static void HandleConfigChange
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    if (le_cfg_QuickGetBool(ConfigPath, true))
    {
        EnableInstantGratification();
    }
    else
    {
        DisableInstantGratification();
    }
}


COMPONENT_INIT
{
    WakeUpSource = le_pm_NewWakeupSource(0, "helloYellow");

    // Register for notification of change of the instant gratification enable/disable
    // configuration in the Config Tree.
    LE_ASSERT(le_cfg_AddChangeHandler(ConfigPath, HandleConfigChange, NULL));

    // If the instant gratification features are enabled, start them up.
    if (le_cfg_QuickGetBool(ConfigPath, true))
    {
        EnableInstantGratification();
    }
}
