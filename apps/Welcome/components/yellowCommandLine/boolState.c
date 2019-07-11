//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Boolean State utility functions.
 */
//--------------------------------------------------------------------------------------------------

#include "cmdLine.h"
#include "boolState.h"

//--------------------------------------------------------------------------------------------------
/**
 * Get a human-readable string representing a given Boolean state.
 *
 * @return A null-terminated, printable string.
 */
//--------------------------------------------------------------------------------------------------
const char* boolState_GetString
(
    boolState_t state
)
//--------------------------------------------------------------------------------------------------
{
    switch (state)
    {
        case BOOL_UNKNOWN:
            return "unknown";
        case BOOL_OFF:
            return "OFF";
        case BOOL_ON:
            return "ON";
    }

    LE_FATAL("Corrupt Boolean state.");
}
