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
#include "instantGratification.h"

static boolState_t VegasModeEnableState = BOOL_UNKNOWN;
static boolState_t GenericLedState = BOOL_UNKNOWN;
static boolState_t TriColourRedState = BOOL_UNKNOWN;
static boolState_t TriColourGreenState = BOOL_UNKNOWN;
static boolState_t TriColourBlueState = BOOL_UNKNOWN;


//--------------------------------------------------------------------------------------------------
/**
 * Enable continuous Vegas Mode.  This is a menu selection call-back.
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
 * Disable continuous Vegas Mode.  This is a menu selection call-back.
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
 * Toggle the generic LED.
 */
//--------------------------------------------------------------------------------------------------
static void ToggleGenericLed
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    DisableVegasMode(NULL);
    bool newState = GenericLedState != BOOL_ON ? true : false;
    dhubAdmin_PushBoolean("/app/leds/mono/enable", 0, newState);
}

//--------------------------------------------------------------------------------------------------
/**
 * Toggle the red in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void ToggleRed
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    DisableVegasMode(NULL);
    bool newState = TriColourRedState != BOOL_ON ? true : false;
    dhubAdmin_PushBoolean("/app/leds/tri/red/enable", 0, newState);
}

//--------------------------------------------------------------------------------------------------
/**
 * Toggle the green in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void ToggleGreen
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    DisableVegasMode(NULL);
    bool newState = TriColourGreenState != BOOL_ON ? true : false;
    dhubAdmin_PushBoolean("/app/leds/tri/green/enable", 0, newState);
}

//--------------------------------------------------------------------------------------------------
/**
 * Toggle the blue in the tri-colour LED.
 */
//--------------------------------------------------------------------------------------------------
static void ToggleBlue
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    DisableVegasMode(NULL);
    bool newState = TriColourBlueState != BOOL_ON ? true : false;
    dhubAdmin_PushBoolean("/app/leds/tri/blue/enable", 0, newState);
}

//--------------------------------------------------------------------------------------------------
/**
 * Enable instant gratification interaction features.
 */
//--------------------------------------------------------------------------------------------------
static void EnableInstantGratification
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    instantGratification_Enable();
    cmdLine_Refresh();
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
    cmdLine_MenuMode();
    cmdLine_AddMenuEntry("Toggle Generic LED", ToggleGenericLed, NULL);
    cmdLine_AddMenuEntry("Toggle Tri-color LED's Red", ToggleRed, NULL);
    cmdLine_AddMenuEntry("Toggle Tri-color LED's Green", ToggleGreen, NULL);
    cmdLine_AddMenuEntry("Toggle Tri-color LED's Blue", ToggleBlue, NULL);

    const char* vegasEnableString = NULL;

    if (instantGratification_IsEnabled())
    {
        switch (VegasModeEnableState)
        {
            case BOOL_OFF:
                vegasEnableString = "DISABLED";
                break;
            case BOOL_ON:
                vegasEnableString = "ENABLED";
                break;
            case BOOL_UNKNOWN:
                vegasEnableString = "in an unknown state";
                break;
        }

        printf("Continuous Vegas Mode is %s\n",
               vegasEnableString);

        if (VegasModeEnableState != BOOL_ON)
        {
            printf("\n"
                   "The LEDs are now under manual control.\n");

            cmdLine_AddMenuEntry("ENABLE Continuous Vegas Mode", EnableVegasMode, NULL);
        }
        else if (VegasModeEnableState == BOOL_ON)
        {
            printf("\n"
                   "The LEDs are being automatically controlled by\n"
                   "Continuous Vegas Mode.  Manual control of the LEDs\n"
                   "using the menu below will disable Continuous Vegas\n"
                   "Mode.\n");

            cmdLine_AddMenuEntry("DISABLE Continuous Vegas Mode", DisableVegasMode, NULL);
        }
    }
    else
    {
        printf("NOTE: Interactive out-of-box experience features are DISABLED.\n"
               "      Vegas mode will not work until these features are re-enabled.\n"
               "      Manual control is still available, however.\n");

        cmdLine_AddMenuEntry("ENABLE interactive out-of-box experience features",
                             EnableInstantGratification,
                             NULL);
    }

    printf("\n"
           "Current state of LEDs:\n"
           "   Generic Green:   %s\n"
           "   Tri-color Red:   %s\n"
           "   Tri-color Green: %s\n"
           "   Tri-color Blue:  %s\n",
           boolState_GetString(GenericLedState),
           boolState_GetString(TriColourRedState),
           boolState_GetString(TriColourGreenState),
           boolState_GetString(TriColourBlueState));
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
    .leaveFunc = Leave,
    .title = "LED Control"
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
