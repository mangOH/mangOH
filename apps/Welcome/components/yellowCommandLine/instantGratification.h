//--------------------------------------------------------------------------------------------------
/**
 * Definitions provided by the Instant Gratification module to other modules.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef INSTANT_GRATIFICATION_H_INCLUDE_GUARD
#define INSTANT_GRATIFICATION_H_INCLUDE_GUARD

//--------------------------------------------------------------------------------------------------
/**
 * Enable instant gratification interaction features.
 */
//--------------------------------------------------------------------------------------------------
void instantGratification_Enable
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Disable instant gratification interaction features.
 */
//--------------------------------------------------------------------------------------------------
void instantGratification_Disable
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * @return true if the instant gratification interaction features are enabled.
 */
//--------------------------------------------------------------------------------------------------
bool instantGratification_IsEnabled
(
    void
);

#endif // INSTANT_GRATIFICATION_H_INCLUDE_GUARD
