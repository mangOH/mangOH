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
           "   Tri-color Blue:  %s\n"
           "\n",
           boolState_GetString(GenericLedState),
           boolState_GetString(TriColourRedState),
           boolState_GetString(TriColourGreenState),
           boolState_GetString(TriColourBlueState));

    if (VegasModeEnableState == BOOL_ON)
    {
        printf("1. DISABLE Continuous Vegas Mode\n");
    }
    else
    {
        printf("1. ENABLE Continuous Vegas Mode\n"
               "2. Turn Generic LED %s\n"
               "3. Turn Tri-color LED Red %s\n"
               "4. Turn Tri-color LED Green %s\n"
               "5. Turn Tri-color LED Blue %s\n",
               GenericLedState == BOOL_ON ? "OFF" : "ON",
               TriColourRedState == BOOL_ON ? "OFF" : "ON",
               TriColourGreenState == BOOL_ON ? "OFF" : "ON",
               TriColourBlueState == BOOL_ON ? "OFF" : "ON");
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a character of input while in the LED screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInput
(
    const char* inputStr
)
//--------------------------------------------------------------------------------------------------
{
    if (VegasModeEnableState == BOOL_ON)
    {
        if (inputStr[0] == '1')
        {
            dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", false);
            VegasModeEnableState = BOOL_OFF;
            cmdLine_Refresh();
        }
        else
        {
            // Eat the invalid selection.
            return;
        }
    }
    else
    {
        // Continuous Vegas Mode is OFF. Manual LED control options are available.
        switch (inputStr[0])
        {
            case '1':
                dhubAdmin_SetBooleanOverride("/app/vegasMode/continuous/enable", true);
                VegasModeEnableState = BOOL_ON;
                break;

            case '2':
                dhubAdmin_PushBoolean("/app/leds/mono/enable", 0, GenericLedState != BOOL_ON);
                break;

            case '3':
                dhubAdmin_PushBoolean("/app/leds/tri/red/enable", 0, TriColourRedState != BOOL_ON);
                break;

            case '4':
                dhubAdmin_PushBoolean("/app/leds/tri/green/enable",
                                      0,
                                      TriColourGreenState != BOOL_ON);
                break;

            case '5':
                dhubAdmin_PushBoolean("/app/leds/tri/blue/enable",
                                      0,
                                      TriColourBlueState != BOOL_ON);
                break;

            default:
                // Eat the invalid selection.
                return;
        }

        cmdLine_Refresh();
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
    drawFunc: Draw,
    inputProcessingFunc: HandleInput,
    leaveFunc: Leave
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

    if (cmdLine_GetCurrentScreen() == &Screen)
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
    cmdLine_MenuMode();
    cmdLine_SetCurrentScreen(&Screen);
}
