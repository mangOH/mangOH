//--------------------------------------------------------------------------------------------------
/**
 * Boolean state definitions used by other modules.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef BOOL_STATE_H_INCLUDE_GUARD
#define BOOL_STATE_H_INCLUDE_GUARD

/// Enumeration of the three [sic] states of a Boolean value.
typedef enum
{
    BOOL_UNKNOWN = -1,
    BOOL_OFF,
    BOOL_ON,
}
boolState_t;


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
);


#endif // BOOL_STATE_H_INCLUDE_GUARD
