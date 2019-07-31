//--------------------------------------------------------------------------------------------------
/**
 * Implementation of control and monitoring of the instant gratification features
 * (blinky lights and button control of buzzer).
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "instantGratification.h"

/// Instant gratification enable/disable configuration path within the Config Tree.
static const char ConfigPath[] = "enableInstantGrat";

//--------------------------------------------------------------------------------------------------
/**
 * Enable instant gratification interaction features.
 */
//--------------------------------------------------------------------------------------------------
void instantGratification_Enable
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    le_cfg_QuickSetBool(ConfigPath, true);
}

//--------------------------------------------------------------------------------------------------
/**
 * Disable instant gratification interaction features.
 */
//--------------------------------------------------------------------------------------------------
void instantGratification_Disable
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    le_cfg_QuickSetBool(ConfigPath, false);
}


//--------------------------------------------------------------------------------------------------
/**
 * @return true if the instant gratification interaction features are enabled.
 */
//--------------------------------------------------------------------------------------------------
bool instantGratification_IsEnabled
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    return le_cfg_QuickGetBool(ConfigPath, true);
}
