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

/// Pointer to an error message string to be displayed on the configuration sub-screen.
/// NULL if no error.
static const char* ErrorMsg = NULL;

static void DrawMain(void);
static void DrawPeriod(void);
static void DrawPercent(void);

static void HandleInputMain(const char* inputStr);
static void HandleInputPeriod(const char* inputStr);
static void HandleInputPercent(const char* inputStr);

static void LeaveMain(void);
static void LeaveSubScreen(void);

static void EnterSubScreen(const Screen_t* screenPtr);

//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t MainScreen =
{
    drawFunc: DrawMain,
    inputProcessingFunc: HandleInputMain,
    leaveFunc: LeaveMain
};

//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the buzzer period screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t PeriodScreen =
{
    drawFunc: DrawPeriod,
    inputProcessingFunc: HandleInputPeriod,
    leaveFunc: LeaveSubScreen
};

//--------------------------------------------------------------------------------------------------
/**
 * Screen object for the buzzer percent screen.
 */
//--------------------------------------------------------------------------------------------------
static const Screen_t PercentScreen =
{
    drawFunc: DrawPercent,
    inputProcessingFunc: HandleInputPercent,
    leaveFunc: LeaveSubScreen
};


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
    printf("\n= Buzzer Control =\n"
           "\n"
           "Current state of the buzzer: %s\n"
           "   Period:     %.3lf seconds\n"
           "   Duty Cycle: %lf %%\n"
           "\n",
           boolState_GetString(EnableState),
           Period,
           Percent);

    printf("1. %s the buzzer\n"
           "2. Set the period\n"
           "3. Set the duty cycle percentage\n",
           EnableState == BOOL_ON ? "DEACTIVATE" : "ACTIVATE");
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a character of input while in the main buzzer screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInputMain
(
    const char* inputStr
)
//--------------------------------------------------------------------------------------------------
{
    switch (inputStr[0])
    {
        case '1':

            dhubAdmin_PushBoolean("/app/buzzer/enable", 0, EnableState != BOOL_ON);
            EnableState = ((EnableState != BOOL_ON) ? BOOL_ON : BOOL_OFF);
            break;

        case '2':
            EnterSubScreen(&PeriodScreen);
            break;

        case '3':
            EnterSubScreen(&PercentScreen);
            break;

        default:

            // Eat the invalid selection.
            return;
    }

    cmdLine_Refresh();
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
    const Screen_t* screenPtr
)
//--------------------------------------------------------------------------------------------------
{
    ErrorMsg = NULL;    // Clear any error from before.

    const char* promptStr;
    if (screenPtr == &PeriodScreen)
    {
        promptStr = "buzzer period";
    }
    else if (screenPtr == &PercentScreen)
    {
        promptStr = "buzzer duty cycle percentage";
    }

    cmdLine_TextEntryMode(promptStr);  // Change to text entry mode.

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
    cmdLine_MenuMode();  // Change back to menu mode.

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
    printf("\n= Buzzer Period Configuration =\n"
           "\n"
           "Current buzzer cycle period: %.3lf seconds\n"
           "\n"
           "Please enter the new buzzer duty cycle period, in seconds.\n"
           "\n"
           "%s\n",
           Period,
           ErrorMsg ? ErrorMsg : "");
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a string of input while in the buzzer period configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInputPeriod
(
    const char* inputStr
)
//--------------------------------------------------------------------------------------------------
{
    // Convert the string into a number.
    char* endPtr;
    errno = 0;
    double period = strtod(inputStr, &endPtr);
    if (errno != 0)
    {
        ErrorMsg = "** ERROR: Number entered is out of range (too big or too small).";
        cmdLine_Refresh();
    }
    else if (*endPtr != '\0')
    {
        ErrorMsg = "** ERROR: Invalid characters in number entered.";
        cmdLine_Refresh();
    }
    else if (endPtr == inputStr)
    {
        // Empty string.  Just exit back to buzzer menu.
        LeaveSubScreen();
    }
    else
    {
        dhubAdmin_SetNumericOverride("/app/buzzer/period", period);
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
    printf("\n= Buzzer Duty Cycle Percentage Configuration =\n"
           "\n"
           "Current duty cycle: %lf %%\n"
           "\n"
           "Please enter the new buzzer duty cycle percentage (a number from 0 - 100).\n"
           "\n"
           "%s\n",
           Percent,
           ErrorMsg ? ErrorMsg : "");
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a string of input while in the buzzer percentage configuration screen.
 */
//--------------------------------------------------------------------------------------------------
static void HandleInputPercent
(
    const char* inputStr
)
//--------------------------------------------------------------------------------------------------
{
    // Convert the string into a number.
    char* endPtr;
    errno = 0;
    double percent = strtod(inputStr, &endPtr);
    if (errno != 0)
    {
        ErrorMsg = "** ERROR: Number entered is out of range (too big or too small).";
        cmdLine_Refresh();
    }
    else if (*endPtr != '\0')
    {
        ErrorMsg = "** ERROR: Invalid characters in number entered.";
        cmdLine_Refresh();
    }
    else if (endPtr == inputStr)
    {
        // Empty string.  Just exit back to buzzer menu.
        LeaveSubScreen();
    }
    else
    {
        dhubAdmin_SetNumericOverride("/app/buzzer/percent", percent);
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
    cmdLine_MenuMode();
    cmdLine_SetCurrentScreen(&MainScreen);
}
