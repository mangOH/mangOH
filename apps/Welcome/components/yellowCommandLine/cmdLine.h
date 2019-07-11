//--------------------------------------------------------------------------------------------------
/**
 * Function and type definitions needed by the implementations of the various screens.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef CMD_LINE_H_INCLUDE_GUARD
#define CMD_LINE_H_INCLUDE_GUARD

#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Screen draw functions look like this.  Each screen must have a draw function, which renders
 * the screen when called.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*DrawFunc_t)
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Screen input processing functions look like this.  The screen's input processing function is
 * called to pass each character of input to the screen for processing.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*InputProcessingFunc_t)
(
    char input
);

//--------------------------------------------------------------------------------------------------
/**
 * Screen leave functions look like this.  The screen's leave function is called to tell the screen
 * to change the current screen to the screen's parent screen.  This happens when the ESC key is
 * pressed by the user.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*LeaveFunc_t)
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * The Screen class.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    DrawFunc_t drawFunc;    /// Function that gets called to draw the screen.
    InputProcessingFunc_t inputProcessingFunc;  /// Function that gets passed input from the user.
    LeaveFunc_t leaveFunc;  /// Function that gets called when the user wants to leave the screen.
}
Screen_t;


//--------------------------------------------------------------------------------------------------
/**
 * Set the current screen to a given screen.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_SetCurrentScreen
(
    const Screen_t* screenPtr
);


//--------------------------------------------------------------------------------------------------
/**
 * Set the current screen to a given screen and call its draw function.
 */
//--------------------------------------------------------------------------------------------------
const Screen_t* cmdLine_GetCurrentScreen
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Refresh the screen by calling the current screen's draw function.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_Refresh
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Print a goodbye message and exit cleanly with a success code.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_Exit
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Put the input processing into menu selection mode.
 * In this mode, 'q' or 'Q' will quit, ESC will cause the screen's Leave function to be called,
 * the numbers '1' through '9' will be passed to the screen's input processing function.
 * Everything else will be ignored.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_MenuMode
(
    void
);


#endif // CMD_LINE_H_INCLUDE_GUARD
