//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the command-line program "hello" for the mangOH Yellow.  This is part of the
 * initial mangOH Yellow user experience.  It provides a menu-driven interface to users, allowing
 * them to
 * - Control sensors and actuators
 * - View sensor readings
 * - Join/leave a WiFi network
 *
 * = Design =
 *
 * The command-line interface receives input from its user via stdin.  A Legato "FD Monitor"
 * is used to receive notification when input is available for processing.  Output intended for the
 * user is written to stdout.
 *
 * The main user interface construct is a screen.  The user interface appears as essentially
 * a tree of screens, rooted at the Main Menu screen.
 *
 * Each screen has static Screen_t instance, which contains a draw function (which is expected to
 * write to stdout) and a "leave" function, which is to be called when the user wants to leave
 * the screen (e.g., when the user presses the ESC key).
 *
 * The CurrentScreenPtr variable holds a pointer to the screen object for current screen.
 *
 * Movement between screens is done by calling cmdLine_SetCurrentScreen().
 *
 * The UI can operate in several different modes.
 *
 * - In menu mode, a menu of between 1 and 9 options are presented to the user and they are asked
 *   to choose one.  This mode is switched to when a screen calls cmdLine_MenuMode(). The screen
 *   is expected to follow their call to cmdLine_MenuMode() with calls to cmdLine_AddMenuEntry()
 *   to add entries to the menu.  The input processing is done by the cmdLine module, who calls
 *   the menu entry selection call-back when an entry is selected.
 *
 * - In text entry mode, the input processing code will buffer and display text entered by the user,
 *   allowing them to edit that text before hitting ENTER to submit the full string for processing
 *   by the screen's entry processing function.  The user can also hit ESC to leave the screen.
 *
 * - Password entry mode (not yet implemented) would be the same as text entry mode, except that
 *   the text entered is masked, appearing as a set of '*' characters.
 *
 * During all modes, an Error Message can be displayed.  cmdLine_SetErrorMsg() and
 * cmdLine_ClearErrorMsg() are used to control this.  The error message is also cleared
 * automatically when changing screens or when a valid menu selection is made or a text or
 * password entry is submitted.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "cmdLine.h"
#include <termios.h>
#include "mainMenu.h"
#include "ledScreen.h"
#include "buzzerScreen.h"
#include "octaveScreen.h"

/// The maximum number of menu items allowed.
/// This is limited by the number of single digits from 1 to 9, inclusive.
/// We don't want to add keypresses for selecting double-digit menu items.
/// We could add 0 as an option, but it is unusual to see an item number 0
/// in a numbered list, and 0 is on the opposite end of the keyboard from 1.
#define MAX_MENU_OPTIONS 9

/// The character that represents end-of-file on this terminal.
static int EndOfFileChar = 4;

/// The character that represents the ESC key on this terminal.
static int EscapeChar = 27;

/// The character that represents the backspace key on this terminal.
static int EraseChar = 127;

/// The character that is received when someone presses CTRL-R.
static int CtrlR = 18;

/// Pointer to the Screen object for the current screen.
static const Screen_t* CurrentScreenPtr;

/// The current input processing mode.
static enum
{
    INPUT_MODE_MENU,            /// See cmdLine_MenuMode().
    INPUT_MODE_TEXT_ENTRY,      /// See cmdLine_TextEntryMode().
    INPUT_MODE_PASSWORD_ENTRY,  /// See cmdLine_PasswordEntryMode().
}
InputProcessingMode = INPUT_MODE_MENU;

/// Buffer containing the error message to be displayed on the screen.
static char ErrorMsg[256] = "";

/// Timer used to wait for further characters after an ESC character, in case it was caused by
/// a non-ESC key press, such as the arrow keys.
static le_timer_Ref_t EscapeTimer;

/// Object in which a single menu entry is stored.
typedef struct
{
    char text[80];   ///< The menu entry's text string (null-terminated).
    cmdLine_MenuSelectionFunc selectionFunc; ///< Function to be called when entry is selected.
    void* contextPtr;   ///< Opaque pointer value to be passed to the selectionFunc.
}
MenuEntry_t;

/// List of Menu Entries.  Index is menu option number minus 1.
static MenuEntry_t MenuEntryList[MAX_MENU_OPTIONS];

/// The number of options in the current menu when in menu mode.
static uint NumMenuOptions = 0;

/// Buffer containing the text prompt to display when in text entry mode or password entry mode.
static char Prompt[64];

/// Function to be called to process a completed text or password entry when the user hits
/// ENTER in text entry mode or password entry mode.
static cmdLine_TextEntryHandlerFunc_t TextEntryHandlerFunc = NULL;

/// Opaque pointer to be passed to the TextEntryHandlerFunc when it is called.
static void* TextEntryHandlerContextPtr = NULL;

/// Buffer containing text entered by the user but not yet submitted.
/// In password entry mode, this just contains '*' characters, and the password is kept in
/// the PasswordBuffer.
static char TextEntryBuffer[1024];

/// Buffer containing actual password characters entered by the user. This must never be displayed.
// static char PasswordBuffer[128];


//--------------------------------------------------------------------------------------------------
/**
 * Draw the current menu.
 */
//--------------------------------------------------------------------------------------------------
static void DrawMenu
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("\n");

    for (uint i = 0; i < NumMenuOptions; i++)
    {
        printf("%u. %s\n", i + 1, MenuEntryList[i].text);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Draw the error message, if there is one.
 */
//--------------------------------------------------------------------------------------------------
static void DrawErrorMsg
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    if (ErrorMsg[0] != '\0')
    {
        printf("\n"
               "%s\n",
               ErrorMsg);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the current screen to a given screen and call its draw function.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_SetCurrentScreen
(
    const Screen_t* screenPtr
)
//--------------------------------------------------------------------------------------------------
{
    CurrentScreenPtr = screenPtr;
    cmdLine_ClearErrorMsg();
    cmdLine_Refresh();
}


//--------------------------------------------------------------------------------------------------
/**
 * Get the current screen.
 */
//--------------------------------------------------------------------------------------------------
const Screen_t* cmdLine_GetCurrentScreen
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    return CurrentScreenPtr;
}


//--------------------------------------------------------------------------------------------------
/**
 * Refresh the screen by calling the current screen's draw function.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_Refresh
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    CurrentScreenPtr->drawFunc(); // WARNING: This may change the input mode and/or menu options.

    switch (InputProcessingMode)
    {
        case INPUT_MODE_MENU:

            DrawMenu();
            DrawErrorMsg();

            printf("\n"
                   "Please make a selection,\n"
                   "or press ESC to leave the screen or Q to quit.\n");
            break;

        case INPUT_MODE_TEXT_ENTRY:
        case INPUT_MODE_PASSWORD_ENTRY:

            DrawErrorMsg();

            printf("\n"
                   "Press ENTER to submit your %s or\n"
                   "press ESC to return to the previous screen.\n"
                   "\n"
                   "%s> %s",
                   Prompt,
                   Prompt,
                   TextEntryBuffer);
            fflush(stdout); // Have to flush because there's no newline at end of what we printed.
            break;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Print a goodbye message and exit cleanly with a success code.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_Exit
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    printf("Goodbye.\n");
    exit(EXIT_SUCCESS);
}


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
)
//--------------------------------------------------------------------------------------------------
{
    InputProcessingMode = INPUT_MODE_MENU;

    // Delete all menu entries.
    NumMenuOptions = 0;
}


//--------------------------------------------------------------------------------------------------
/**
 * Add an entry to the menu.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_AddMenuEntry
(
    const char* text, ///< Null-terminated string containing the text of the entry (will be copied).
    cmdLine_MenuSelectionFunc selectionFunc, ///< Function to be called when entry is selected.
    void* contextPtr  ///< Opaque pointer value to be passed to the selectionFunc.
)
//--------------------------------------------------------------------------------------------------
{
    LE_ASSERT(NumMenuOptions < MAX_MENU_OPTIONS);

    MenuEntry_t* entryPtr = MenuEntryList + NumMenuOptions;

    le_result_t code = le_utf8_Copy(entryPtr->text, text, sizeof(entryPtr->text), NULL);
    LE_WARN_IF(code != LE_OK, "Failed (%s) to copy menu entry '%s'.", LE_RESULT_TXT(code), text);
    entryPtr->selectionFunc = selectionFunc;
    entryPtr->contextPtr = contextPtr;

    NumMenuOptions++;
}


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
)
//--------------------------------------------------------------------------------------------------
{
    InputProcessingMode = INPUT_MODE_TEXT_ENTRY;
    (void)snprintf(Prompt, sizeof(Prompt), "%s", promptStr);
    TextEntryHandlerFunc = handlerFunc;
    TextEntryHandlerContextPtr = contextPtr;
    memset(TextEntryBuffer, 0, sizeof(TextEntryBuffer));
}


//--------------------------------------------------------------------------------------------------
/**
 * Set the error message (displayed at the bottom of the screen) to a specific string.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_SetErrorMsg
(
    const char* msgPtr  ///< Ptr to the null-terminated message string (which will be copied).
)
//--------------------------------------------------------------------------------------------------
{
    (void)snprintf(ErrorMsg, sizeof(ErrorMsg), "** ERROR: %s", msgPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Clear the error message.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_ClearErrorMsg
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    ErrorMsg[0] = '\0';
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a character of input while in Menu Mode.
 */
//--------------------------------------------------------------------------------------------------
static void HandleMenuInput
(
    char input
)
//--------------------------------------------------------------------------------------------------
{
    char highestValidOption = '0' + NumMenuOptions;

    if ((input == 'q') || (input == 'Q'))
    {
        cmdLine_Exit();
    }
    else if ((input >= '1') && (input <= highestValidOption))
    {
        cmdLine_ClearErrorMsg();

        MenuEntry_t* entryPtr = &(MenuEntryList[input - '1']);
        entryPtr->selectionFunc(entryPtr->contextPtr);
    }
    else if ((input > highestValidOption) && (input <= '9'))
    {
        cmdLine_SetErrorMsg("Invalid selection. Please choose again.");
        cmdLine_Refresh();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Handle a character of input while in Text Entry Mode.
 */
//--------------------------------------------------------------------------------------------------
static void HandleTextInput
(
    char input
)
//--------------------------------------------------------------------------------------------------
{
    // Handle backspace
    if (input == EraseChar)
    {
        size_t len = strnlen(TextEntryBuffer, sizeof(TextEntryBuffer));

        if (len > 0)
        {
            TextEntryBuffer[len - 1] = '\0';

            cmdLine_Refresh();
        }
    }
    // Check for newline or carriage return, and treat those as ENTER, passing the string
    // to the screen for processing.
    else if ((input == '\n') || (input == '\r'))
    {
        cmdLine_ClearErrorMsg();

        TextEntryHandlerFunc(TextEntryBuffer, TextEntryHandlerContextPtr);
    }
    // Ignore all non-printable characters.
    else if (isprint(input))
    {
        size_t len = strnlen(TextEntryBuffer, sizeof(TextEntryBuffer));
        if (len < (sizeof(TextEntryBuffer) - 1))
        {
            TextEntryBuffer[len] = input;
        }

        cmdLine_Refresh();
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * ESC key timer expiry handler function.  If this expires, it means that no other characters
 * were received after an ESC key, so it must have actually been an ESC key press and not something
 * like an arrow key press.
 */
//--------------------------------------------------------------------------------------------------
static void HandleEscapeTimerExpiry
(
    le_timer_Ref_t timer
)
//--------------------------------------------------------------------------------------------------
{
    CurrentScreenPtr->leaveFunc();
}


//--------------------------------------------------------------------------------------------------
/**
 * Call-back function that gets called when input is available on stdin.
 */
//--------------------------------------------------------------------------------------------------
static void InputHandler
(
    int fd,
    short events    ///< POLLIN, POLLRDHUP, POLLERR or POLLHUP
)
//--------------------------------------------------------------------------------------------------
{
    // If there's data to be read,
    if (events & POLLIN)
    {
        char input;

        ssize_t bytesRead = read(fd, &input, sizeof(input));

        if (bytesRead == sizeof(input))
        {
            // If the escape key timer is running, stop it.
            le_timer_Stop(EscapeTimer);

            if (input == EndOfFileChar)
            {
                cmdLine_Exit();
            }
            else if (input == EscapeChar)
            {
                // Start the ESC key timer.
                le_timer_Start(EscapeTimer);
            }
            else if (input == CtrlR)
            {
                cmdLine_Refresh();
            }
            else
            {
                switch (InputProcessingMode)
                {
                    case INPUT_MODE_MENU:
                        HandleMenuInput(input);
                        break;

                    case INPUT_MODE_TEXT_ENTRY:
                        HandleTextInput(input);
                        break;

                    case INPUT_MODE_PASSWORD_ENTRY:
                        LE_FATAL("Password entry mode not implemented.");
                        break;
                }
            }
        }
        else if (bytesRead < 0)
        {
            LE_ERROR("Error reading stdin: %m");
        }
        else
        {
            // End of file.
            exit(EXIT_SUCCESS);
        }
    }

    // Check for errors.
    if (events & (POLLRDHUP | POLLERR | POLLHUP))
    {
        fprintf(stderr, "Input stream interrupted.\n");
        exit(EXIT_FAILURE);
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Configure the terminal.
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureTerminal
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Set up an FD Monitor to monitor stdin.
    le_fdMonitor_Create("input", 0, InputHandler, POLLIN);

    // Set stdin non-blocking.
    int fileFlags = fcntl(0, F_GETFL);
    if (fileFlags < 0)
    {
        LE_FATAL("Unable to get file flags for stdin: %m");
    }
    fileFlags |= O_NONBLOCK;
    if (fcntl(0, F_SETFL, fileFlags) < 0)
    {
        LE_FATAL("Unable to set stdin non-blocking: %m");
    }

    // Switch the terminal to raw mode.
    struct termios termSettings;
    if (tcgetattr(0, &termSettings) != 0)
    {
        LE_FATAL("Couldnt get terminal settings: %m");
    }
    termSettings.c_lflag &= ~(ICANON | ECHO); // Turn off canonical (cooked) mode and echo.
    termSettings.c_cc[VMIN] = 0;    // Don't wait for a minimum number of characters.
    termSettings.c_cc[VTIME] = 0;   // Don't wait for a minimum number of deciseconds.
    if (tcsetattr(0, TCSANOW /* apply changes immediately */, &termSettings) != 0)
    {
        LE_FATAL("Couldnt set terminal settings: %m");
    }

    // Remember what the terminal's EOF and backspace characters are so we can check for them later.
    EndOfFileChar = termSettings.c_cc[VEOF];
    EraseChar = termSettings.c_cc[VERASE];

    LE_INFO("End of file char = %d.", EndOfFileChar);
    LE_INFO("Erase char = %d.", EraseChar);
}


COMPONENT_INIT
{
    EscapeTimer = le_timer_Create("ESC timer");
    le_timer_SetMsInterval(EscapeTimer, 100);
    le_timer_SetHandler(EscapeTimer, HandleEscapeTimerExpiry);

    ConfigureTerminal();

    // Initialize all the screens.
    mainMenu_Init();
    ledScreen_Init();
    buzzerScreen_Init();
    octaveScreen_Init();

    // Print a friendly welcome message followed by the main menu.
    printf("\n"
           "Welcome to mangOH Yellow!\n");
    mainMenu_Enter();
}
