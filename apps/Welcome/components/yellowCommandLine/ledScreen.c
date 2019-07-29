//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the LED Control screen.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "cmdLine.h"
#include "ledScreen.h"
#include "mainMenu.h"
#include "boolState.h"

static boolState_t VegasModeEnableState = BOOL_UNKNOWN;
static boolState_t GenericLedState = BOOL_UNKNOWN;
static boolState_t TriColourRedState = BOOL_UNKNOWN;
static boolState_t TriColourGreenState = BOOL_UNKNOWN;
static boolState_t TriColourBlueState = BOOL_UNKNOWN;


//--------------------------------------------------------------------------------------------------
/**
 * Enable continuous Vegas Mode.
 */
//--------------------------------------------------------------------------------------------------
static void EnableVegasMode
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Disable continuous Vegas Mode.
 */
//--------------------------------------------------------------------------------------------------
static void DisableVegasMode
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn on the generic LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnGenericLedOn
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/mono/enable", 0, true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn off the generic LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnGenericLedOff
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/mono/enable", 0, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn on the red in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnRedOn
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/red/enable", 0, true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn off the red in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnRedOff
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/red/enable", 0, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn on the green in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnGreenOn
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/green/enable", 0, true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn off the green in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnGreenOff
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/green/enable", 0, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn on the blue in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnBlueOn
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/blue/enable", 0, true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Turn off the blue in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void TurnBlueOff
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/leds/tri/blue/enable", 0, false);
}

//--------------------------------------------------------------------------------------------------
/**
 * Draw the LED Control screen.
 */
//--------------------------------------------------------------------------------------------------
static void Draw
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("\n= LED Control =\n"
           "\n"
           "Current state of LEDs:\n"
           "   Generic Green:   %s\n"
           "   Tri-color Red:   %s\n"
           "   Tri-color Green: %s\n"
           "   Tri-color Blue:  %s\n",
           boolState_GetString(GenericLedState),
           boolState_GetString(TriColourRedState),
           boolState_GetString(TriColourGreenState),
           boolState_GetString(TriColourBlueState));

    if (VegasModeEnableState != BOOL_OFF)
    {
        cmdLine_MenuMode();
        cmdLine_AddMenuEntry("DISABLE Continuous Vegas Mode", DisableVegasMode, NULL);

        printf("\n"
               "NOTE: Other options become available when continuous\n"
               "      Vegas Mode is disabled.\n");
    }
    else
    {
        cmdLine_MenuMode();
        cmdLine_AddMenuEntry("ENABLE Continuous Vegas Mode", EnableVegasMode, NULL);

        if (GenericLedState == BOOL_ON)
        {
            cmdLine_AddMenuEntry("Turn Generic LED OFF", TurnGenericLedOff, NULL);
        }
        else
        {
            cmdLine_AddMenuEntry("Turn Generic LED ON", TurnGenericLedOn, NULL);
        }

        if (TriColourRedState == BOOL_ON)
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Red OFF", TurnRedOff, NULL);
        }
        else
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Red ON", TurnRedOn, NULL);
        }

        if (TriColourGreenState == BOOL_ON)
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Green OFF", TurnGreenOff, NULL);
        }
        else
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Green ON", TurnGreenOn, NULL);
        }

        if (TriColourBlueState == BOOL_ON)
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Blue OFF", TurnBlueOff, NULL);
        }
        else
        {
            cmdLine_AddMenuEntry("Turn Tri-color LED's Blue ON", TurnBlueOn, NULL);
        }
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Leave the LED screen.
 */
//--------------------------------------------------------------------------------------------------
static void Leave
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    mainMenu_Enter();
}


//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t Screen =
{
    .drawFunc = Draw,
    .leaveFunc = Leave
};


//--------------------------------------------------------------------------------------------------
/**
 * Data Hub push handler for the LED states.
 */
//--------------------------------------------------------------------------------------------------
static void LedPushHandler
(
    double timestamp,
    bool value,     ///< true = LED is on.
    void* contextPtr ///< Ptr to the boolean state variable for this LED.
)
//--------------------------------------------------------------------------------------------------
{
    boolState_t* stateVarPtr = contextPtr;

    *stateVarPtr = value ? BOOL_ON : BOOL_OFF;

    const Screen_t* screenPtr = cmdLine_GetCurrentScreen();
    if (screenPtr == &Screen)
    {
        cmdLine_Refresh();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the LED Screen module.
 */
//--------------------------------------------------------------------------------------------------
void ledScreen_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Register to receive data samples from the Data Hub for all data points we care about.

    // LED states
    dhubAdmin_AddBooleanPushHandler("/app/leds/mono/enable",
                                    LedPushHandler,
                                    &GenericLedState);
    dhubAdmin_AddBooleanPushHandler("/app/leds/tri/red/enable",
                                    LedPushHandler,
                                    &TriColourRedState);
    dhubAdmin_AddBooleanPushHandler("/app/leds/tri/green/enable",
                                    LedPushHandler,
                                    &TriColourGreenState);
    dhubAdmin_AddBooleanPushHandler("/app/leds/tri/blue/enable",
                                    LedPushHandler,
                                    &TriColourBlueState);

    // Vegas Mode enabled
    // NOTE: Reuses LED state handlers.
    dhubAdmin_AddBooleanPushHandler("/app/vegasMode/continuous/enable",
                                    LedPushHandler,
                                    &VegasModeEnableState);
}


//--------------------------------------------------------------------------------------------------
/**
 * Enter the LED Screen.
 */
//--------------------------------------------------------------------------------------------------
void ledScreen_Enter
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    cmdLine_SetCurrentScreen(&Screen);
}
