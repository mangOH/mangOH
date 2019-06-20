//--------------------------------------------------------------------------------------------------
/**
 * Top-level implementation file for LED control service for mangOH Yellow.
 *
 * Creates control resources in the Data Hub, so the Data Hub can be used to control the LEDs
 * on the mangOH Yellow.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#include "outputActuator.h"

COMPONENT_INIT
{
    outputActuator_Register("tri/red", "/sys/devices/platform/expander.0/tri_led_red");
    outputActuator_Register("tri/green", "/sys/devices/platform/expander.0/tri_led_grn");
    outputActuator_Register("tri/blue", "/sys/devices/platform/expander.0/tri_led_blu");
    outputActuator_Register("mono", "/sys/devices/platform/expander.0/generic_led");
}
