//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the command-line program "hello" for the mangOH Yellow.  This is part of the
 * initial mangOH Yellow user experience.  It provides a menu-driven interface to users, allowing
 * them to
 * - Join/leave a WiFi network
 * - Check for software updates (when an Internet connection is available)
 *
 * In particular, joining a WiFi network and checking for updates is key because the time between
 * manufacturing and delivery to the customer will sometimes be many months, which virtually
 * guarantees that the software/firmware delivered on-board the device will be out of date and
 * likely to contain known defects that could degrade the initial user experience and/or leave the
 * device vulnerable to remote attack.
 *
 * = Design =
 *
 * The command-line interface receives input from its user via stdin.  A Legato "FD Monitor"
 * is used to receive notification when input is available for processing.  Output intended for the
 * user is written to stdout.
 *
 * The main user interface construct is a screen.  The user interface is essentially a tree of
 * screens, rooted at the Main Menu screen.
 *
 * Each screen has static Screen_t instance, which contains a draw function (which is expected to
 * write to stdout) and an input processing function (to which input from stdin will be passed for
 * processing).
 *
 * The CurrentScreenPtr variable holds a pointer to the screen object for current screen.
 *
 * Movement between screens is done by calling cmdLine_SetCurrentScreen().
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "cmdLine.h"
#include <termios.h>
#include "mainMenu.h"
#include "ledScreen.h"
#include "buzzerScreen.h"

/// The character that represents end-of-file on this terminal.
static int EndOfFileChar = 4;

/// The character that represents the ESC key on this terminal.
static int EscapeChar = 27;

/// The character that represents the backspace key on this terminal.
static int EraseChar = 127;

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

/// Text prompt to display when in text entry mode or password entry mode.
static char Prompt[32];

/// Buffer containing text entered by the user but not yet submitted.
/// In password entry mode, this just contains '*' characters, and the password is kept in
/// the PasswordBuffer.
static char TextEntryBuffer[1024];

/// Buffer containing actual password characters entered by the user. This must never be displayed.
// static char PasswordBuffer[128];


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
    CurrentScreenPtr->drawFunc();

    switch (InputProcessingMode)
    {
        case INPUT_MODE_MENU:

            printf("\n"
                   "Please make a selection,\n"
                   "or press ESC to leave the screen or Q to quit.\n");
            break;

        case INPUT_MODE_TEXT_ENTRY:
        case INPUT_MODE_PASSWORD_ENTRY:

            printf("\n"
                   "%s %s\n"
                   "\n"
                   "Press ENTER to submit your %s or\n"
                   "press ESC to return to the previous screen.\n",
                   Prompt,
                   TextEntryBuffer,
                   Prompt);
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
 * the numbers '1' through '9' will be passed to the screen's input processing function.
 * Everything else will be ignored.
 */
//--------------------------------------------------------------------------------------------------
void cmdLine_MenuMode
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    InputProcessingMode = INPUT_MODE_MENU;
}


//--------------------------------------------------------------------------------------------------
/**
 * @return true if the given character is the ESC key character.
 */
//--------------------------------------------------------------------------------------------------
bool cmdLine_IsEscapeChar
(
    char input
)
//--------------------------------------------------------------------------------------------------
{
    return (input == EscapeChar);
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
    if ((input == 'q') || (input == 'Q'))
    {
        cmdLine_Exit();
    }
    else if ((input >= '1') && (input <= '9'))
    {
        CurrentScreenPtr->inputProcessingFunc(input);
    }
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
            if (input == EndOfFileChar)
            {
                cmdLine_Exit();
            }
            else if (input == EscapeChar)
            {
                CurrentScreenPtr->leaveFunc();
            }
            else
            {
                switch (InputProcessingMode)
                {
                    case INPUT_MODE_MENU:
                        HandleMenuInput(input);
                        break;

                    case INPUT_MODE_TEXT_ENTRY:
                        LE_FATAL("Text entry mode not implemented.");
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
}


COMPONENT_INIT
{
    ConfigureTerminal();

    // Initialize all the screens.
    mainMenu_Init();
    ledScreen_Init();
    buzzerScreen_Init();

    // Print a friendly welcome message followed by the main menu.
    printf("\n"
           "Welcome to mangOH Yellow!\n");
    mainMenu_Enter();
}
