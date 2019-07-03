#include "legato.h"
#include "interfaces.h"

//--------------------------------------------------------------------------------------------------
/**
 * Event handler call-back that gets called when the button state changes.
 */
//--------------------------------------------------------------------------------------------------
static void ButtonStateChangeHandler
(
    bool isActive,      ///< true = the button is down, false = the button is up.
    void* contextPtr    ///< Not used.
)
//--------------------------------------------------------------------------------------------------
{
    dhubIO_PushBoolean("value", DHUBIO_NOW, isActive);
}


COMPONENT_INIT
{
    // Configure the button's GPIO as an active-low input.
    LE_ASSERT_OK(gpio_SetInput(GPIO_ACTIVE_LOW));

    // Register a change event callback and enable reports on state changes.
    LE_ASSERT(gpio_AddChangeEventHandler(GPIO_EDGE_BOTH, ButtonStateChangeHandler, NULL, 100));
    LE_ASSERT_OK(gpio_SetEdgeSense(GPIO_EDGE_BOTH));

    // Create a Boolean input to the Data Hub to indicate the current state of the button.
    LE_ASSERT_OK(dhubIO_CreateInput("value", DHUBIO_DATA_TYPE_BOOLEAN, ""));
}
