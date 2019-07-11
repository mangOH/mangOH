//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Buzzer Control screen.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "cmdLine.h"
#include "buzzerScreen.h"
#include "mainMenu.h"
#include "boolState.h"

/// The state of the buzzer's enable output.
static boolState_t EnableState = BOOL_UNKNOWN;

/// The buzzer's period.
static double Period = 0;

/// The buzzer's duty cycle percentage.
static double Percent = 0;


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
    printf("\n= Buzzer Control =\n"
           "\n"
           "Current state of the buzzer: %s\n"
           "   Period:     %lf\n"
           "   Duty Cycle: %lf\n"
           "\n",
           boolState_GetString(EnableState),
           Period,
           Percent);

    printf("1. %s the buzzer\n"
           "2. Set the period\n"
           "3. Set the duty cycle percentage\n",
           EnableState == BOOL_ON ? "ACTIVATE" : "DEACTIVATE");
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a character of input while in the LED screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInput
(
    char input
)
//--------------------------------------------------------------------------------------------------
{
    switch (input)
    {
        case '1':

            dhubAdmin_PushBoolean("/app/buzzer/enable", 0, EnableState != BOOL_ON);
            EnableState = ((EnableState != BOOL_ON) ? BOOL_ON : BOOL_OFF);
            break;

        case '2':

            printf("Sorry, period configuration is not yet implemented.\n");
            break;

        case '3':
            printf("Sorry, duty cycle percentage configuration is not yet implemented.\n");
            break;

        default:

            // Eat the invalid selection.
            return;
    }

    cmdLine_Refresh();
}


//--------------------------------------------------------------------------------------------------
/**
 * Leave the screen.
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
 * Data Hub push handler for the enable state.
 */
//--------------------------------------------------------------------------------------------------
static void EnablePushHandler
(
    double timestamp,
    bool value,     ///< true = buzzer is active.
    void* contextPtr ///< Not used
)
//--------------------------------------------------------------------------------------------------
{
    EnableState = value ? BOOL_ON : BOOL_OFF;

    if (cmdLine_GetCurrentScreen() == &Screen)
    {
        cmdLine_Refresh();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Data Hub push handler for the numeric values (period and percent).
 */
//--------------------------------------------------------------------------------------------------
static void NumberPushHandler
(
    double timestamp,
    double value,
    void* contextPtr ///< Pointer to the variable to update.
)
//--------------------------------------------------------------------------------------------------
{
    double* numberPtr = contextPtr;

    *numberPtr = value;

    if (cmdLine_GetCurrentScreen() == &Screen)
    {
        cmdLine_Refresh();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Initialize the Buzzer Screen module.
 */
//--------------------------------------------------------------------------------------------------
void buzzerScreen_Init
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Register to receive data samples from the Data Hub for all data points we care about.

    dhubAdmin_AddBooleanPushHandler("/app/buzzer/enable",
                                    EnablePushHandler,
                                    NULL);
    dhubAdmin_AddNumericPushHandler("/app/buzzer/period",
                                    NumberPushHandler,
                                    &Period);
    dhubAdmin_AddNumericPushHandler("/app/buzzer/percent",
                                    NumberPushHandler,
                                    &Percent);
}


//--------------------------------------------------------------------------------------------------
/**
 * Enter the screen.
 */
//--------------------------------------------------------------------------------------------------
void buzzerScreen_Enter
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    cmdLine_MenuMode();
    cmdLine_SetCurrentScreen(&Screen);
}
