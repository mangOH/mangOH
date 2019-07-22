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
 * the numbers '1' through '9' will be handled as menu selections, triggering a call-back to
 * the menu selection handler function or reporting an invalid selection error, depending on the
 * current menu configuration (see cmdLine_AddMenuEntry()).  All other keypresses will be ignored.
 *
 * @note After calling this function, the menu will be empty.  Call cmdLine_AddMenuEntry() to add
 *       entries to the menu.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_MenuMode
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Functions that are called to indicate to a screen that a particular menu item was selected
 * must look like this.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*cmdLine_MenuSelectionFunc)
(
    void* contextPtr    ///< Opaque pointer value passed into cmdLine_AddMenuEntry().
);

//--------------------------------------------------------------------------------------------------
/**
 * Add an entry to the menu.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_AddMenuEntry
(
    const char* text, ///< Null-terminated string containing the text of the entry (will be copied).
    cmdLine_MenuSelectionFunc selectionFunc, ///< Function to be called when entry is selected.
    void* contextPtr;   ///< Opaque pointer value to be passed to the selectionFunc.
);


//--------------------------------------------------------------------------------------------------
/**
 * Functions called-back to process text or a password entered by the user in text entry mode or
 * password entry mode must look like this.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*cmdLine_TextEntryHandlerFunc_t)
(
    const char* text,   ///< Ptr to the null-terminated text entered by the user.
    void* contextPtr    ///< Opaque pointer passed to cmdLine_TextEntryMode() or
                        ///< cmdLine_PasswordEntryMode().
);


//--------------------------------------------------------------------------------------------------
/**
 * Put the input processing into text entry mode.
 * In this mode, ESC will cause the screen's Leave function to be called,
 * other keys will be applied to the TextEntryBuffer and the contents of that buffer will be
 * echoed to the screen.  When ENTER is pressed, the contents of the TextEntryBuffer will be
 * passed to the provided handler function.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_TextEntryMode
(
    const char* promptStr, ///< The name of the item to be entered. E.g., "buzzer period".
    cmdLine_TextEntryHandlerFunc_t handlerFunc, ///< Function to be called when ENTER is pressed.
    void* contextPtr ///< Opaque pointer to be passed to the handlerFunc when it is called.
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the error message (displayed at the bottom of the screen) to a specific string.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_SetErrorMsg
(
    const char* msgPtr  ///< Ptr to the null-terminated message string (which will be copied).
);

//--------------------------------------------------------------------------------------------------
/**
 * Clear the error message.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_ClearErrorMsg
(
    void
);


#endif // CMD_LINE_H_INCLUDE_GUARD
