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

static void DrawMain(void);
static void DrawPeriod(void);
static void DrawPercent(void);

static void HandleInputPeriod(const char* inputStr, void* contextPtr);
static void HandleInputPercent(const char* inputStr, void* contextPtr);

static void LeaveMain(void);
static void LeaveSubScreen(void);

static void EnterSubScreen(void* contextPtr);


//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t MainScreen =
{
    .drawFunc = DrawMain,
    .leaveFunc = LeaveMain,
    .title = "Buzzer Control"
};

//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the buzzer period screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t PeriodScreen =
{
    .drawFunc = DrawPeriod,
    .leaveFunc = LeaveSubScreen,
    .title = "Buzzer Period Configuration"
};

//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the buzzer percent screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t PercentScreen =
{
    .drawFunc = DrawPercent,
    .leaveFunc = LeaveSubScreen,
    .title = "Buzzer Duty Cycle Percentage Configuration"
};


//--------------------------------------------------------------------------------------------------
/**
 * Activate the buzzer.
 */
//--------------------------------------------------------------------------------------------------
static void ActivateBuzzer
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/buzzer/enable", 0, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Deactivate the buzzer.
 */
//--------------------------------------------------------------------------------------------------
static void DeactivateBuzzer
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    dhubAdmin_PushBoolean("/app/buzzer/enable", 0, false);
}


//--------------------------------------------------------------------------------------------------
/**
 * Draw the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static void DrawMain
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("Current state of the buzzer: %s\n"
           "\n"
           "   Period:     %.3lf seconds\n"
           "   Duty Cycle: %lf %%\n",
           boolState_GetString(EnableState),
           Period,
           Percent);

    cmdLine_MenuMode();
    if (EnableState == BOOL_OFF)
    {
        cmdLine_AddMenuEntry("ACTIVATE the buzzer", ActivateBuzzer, NULL);
    }
    else
    {
        cmdLine_AddMenuEntry("DEACTIVATE the buzzer", DeactivateBuzzer, NULL);
    }
    cmdLine_AddMenuEntry("Set the period", EnterSubScreen, &PeriodScreen);
    cmdLine_AddMenuEntry("Set the duty cycle percentage", EnterSubScreen, &PercentScreen);
}


//--------------------------------------------------------------------------------------------------
/**
 * Leave the main buzzer screen (return to the main menu).
 */
//--------------------------------------------------------------------------------------------------
static void LeaveMain
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    mainMenu_Enter();
}


//--------------------------------------------------------------------------------------------------
/**
 * Enter a buzzer configuration screen, which is a sub-screen under the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static void EnterSubScreen
(
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    const Screen_t* screenPtr = contextPtr;

    if (screenPtr == &PeriodScreen)
    {
        cmdLine_TextEntryMode("buzzer period", HandleInputPeriod, NULL);
    }
    else if (screenPtr == &PercentScreen)
    {
        cmdLine_TextEntryMode("buzzer duty cycle percentage", HandleInputPercent, NULL);
    }

    cmdLine_SetCurrentScreen(screenPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Leave a buzzer configuration screen, which is a sub-screen under the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static void LeaveSubScreen
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    cmdLine_SetCurrentScreen(&MainScreen);
}


//--------------------------------------------------------------------------------------------------
/**
 * Draw the buzzer period configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void DrawPeriod
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("Current buzzer cycle period: %.3lf seconds\n"
           "\n"
           "Please enter the new buzzer duty cycle period, in seconds.\n",
           Period);
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a string of input while in the buzzer period configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInputPeriod
(
    const char* inputStr,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    // Convert the string into a number.
    char* endPtr;
    errno = 0;
    double period = strtod(inputStr, &endPtr);
    if (errno != 0)
    {
        cmdLine_SetErrorMsg("Number entered is out of range (too big or too small).");
        cmdLine_Refresh();
    }
    else if (*endPtr != '\0')
    {
        cmdLine_SetErrorMsg("Invalid characters in number entered.");
        cmdLine_Refresh();
    }
    else if (endPtr == inputStr)
    {
        // Empty string.  Just exit back to buzzer menu.
        LeaveSubScreen();
    }
    else if (period < 0.001)
    {
        cmdLine_SetErrorMsg("Period entered is less than 1 ms.");
        cmdLine_Refresh();
    }
    else
    {
        dhubAdmin_PushNumeric("/app/buzzer/period", 0, period);
        LeaveSubScreen();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Draw the buzzer percent configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void DrawPercent
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("Current duty cycle: %lf %%\n"
           "\n"
           "Please enter the new buzzer duty cycle percentage (a number from 0 - 100).\n",
           Percent);
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a string of input while in the buzzer percentage configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInputPercent
(
    const char* inputStr,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    // Convert the string into a number.
    char* endPtr;
    errno = 0;
    double percent = strtod(inputStr, &endPtr);
    if (errno != 0)
    {
        cmdLine_SetErrorMsg("Number entered is out of range (too big or too small).");
        cmdLine_Refresh();
    }
    else if (*endPtr != '\0')
    {
        cmdLine_SetErrorMsg("Invalid characters in number entered.");
        cmdLine_Refresh();
    }
    else if (endPtr == inputStr)
    {
        // Empty string.  Just exit back to buzzer menu.
        LeaveSubScreen();
    }
    else if (percent > 100.0)
    {
        cmdLine_SetErrorMsg("Number entered is greater than 100%.");
        cmdLine_Refresh();
    }
    else if (percent < 0.0)
    {
        cmdLine_SetErrorMsg("Number entered is less than 0%.");
        cmdLine_Refresh();
    }
    else
    {
        dhubAdmin_PushNumeric("/app/buzzer/percent", 0, percent);
        LeaveSubScreen();
    }
}


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

    if (cmdLine_GetCurrentScreen() == &MainScreen)
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

    if (cmdLine_GetCurrentScreen() == &MainScreen)
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
    cmdLine_SetCurrentScreen(&MainScreen);
}
