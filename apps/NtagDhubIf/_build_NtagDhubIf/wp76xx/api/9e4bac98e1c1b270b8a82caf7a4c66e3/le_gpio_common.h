
/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */
#ifndef LE_GPIO_COMMON_H_INCLUDE_GUARD
#define LE_GPIO_COMMON_H_INCLUDE_GUARD


#include "legato.h"

#define IFGEN_LE_GPIO_PROTOCOL_ID "eb4b6dc575d64f27484e7657275f11fd"
#define IFGEN_LE_GPIO_MSG_SIZE 20



//--------------------------------------------------------------------------------------------------
/**
 * Pin polarities.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_GPIO_ACTIVE_HIGH = 0,
        ///< GPIO active-high, output is 1
    LE_GPIO_ACTIVE_LOW = 1
        ///< GPIO active-low, output is 0
}
le_gpio_Polarity_t;


//--------------------------------------------------------------------------------------------------
/**
 * Edge transitions.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_GPIO_EDGE_NONE = 0,
        ///< No edge detection
    LE_GPIO_EDGE_RISING = 1,
        ///< Notify when voltage goes from low to high.
    LE_GPIO_EDGE_FALLING = 2,
        ///< Notify when voltage goes from high to low.
    LE_GPIO_EDGE_BOTH = 3
        ///< Notify when pin voltage changes state in either direction.
}
le_gpio_Edge_t;


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'le_gpio_ChangeEvent'
 */
//--------------------------------------------------------------------------------------------------
typedef struct le_gpio_ChangeEventHandler* le_gpio_ChangeEventHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Pull up or down.
 */
//--------------------------------------------------------------------------------------------------
typedef enum
{
    LE_GPIO_PULL_OFF = 0,
        ///< Neither resistor is enabled
    LE_GPIO_PULL_DOWN = 1,
        ///< Pulldown resistor is enabled
    LE_GPIO_PULL_UP = 2
        ///< Pullup resistor is enabled
}
le_gpio_PullUpDown_t;


//--------------------------------------------------------------------------------------------------
/**
 * State change event handler (callback).
 */
//--------------------------------------------------------------------------------------------------
typedef void (*le_gpio_ChangeCallbackFunc_t)
(
        bool state,
        ///< New state of pin (true = active, false = inactive).
        void* contextPtr
        ///<
);


//--------------------------------------------------------------------------------------------------
/**
 * Get if this client bound locally.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_le_gpio_HasLocalBinding
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Init data that is common across all threads
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_le_gpio_InitCommonData
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Perform common initialization and open a session
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_OpenSession
(
    le_msg_SessionRef_t _ifgen_sessionRef,
    bool isBlocking
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as an input pin.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetInput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Polarity_t polarity
        ///< [IN] Active-high or active-low.
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as a push-pull output pin.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetPushPullOutput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Polarity_t polarity,
        ///< [IN] Active-high or active-low.
        bool value
        ///< [IN] Initial value to drive (true = active, false = inactive)
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as a tri-state output pin.
 *
 * @note The initial state will be high-impedance.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetTriStateOutput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Polarity_t polarity
        ///< [IN] Active-high or active-low.
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as an open-drain output pin.  "High" is a high-impedance state, while "Low"
 * pulls the pin to ground.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetOpenDrainOutput
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Polarity_t polarity,
        ///< [IN] Active-high or active-low.
        bool value
        ///< [IN] Initial value to drive (true = active, false = inactive)
);

//--------------------------------------------------------------------------------------------------
/**
 * Enable the pull-up resistor (disables pull-down if previously enabled).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_EnablePullUp
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Enable the pull-down resistor (disables pull-up if previously enabled).
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_EnablePullDown
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Disable the pull-up and pull-down resistors.  Does nothing if both are already disabled.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_DisableResistors
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to active state.
 *
 * @warning Only valid for output pins.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_Activate
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to inactive state.
 *
 * @warning Only valid for output pins.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_Deactivate
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to high impedance state.
 *
 * @warning Only valid for tri-state or open-drain output pins.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetHighZ
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Read value of GPIO input pin.
 *
 * @return true = active, false = inactive.
 *
 * @note It is invalid to read an output pin.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_le_gpio_Read
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_gpio_ChangeEvent'
 *
 * Register a callback function to be called when an input pin changes state.
 *
 * If the pin is not capable of interrupt-driven operation, then it will be sampled every
 * sampleMs milliseconds.  Otherwise, sampleMs will be ignored.
 *
 * If this fails, either because the handler cannot be registered, or setting the
 * edge detection fails, then it will return a NULL reference.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_gpio_ChangeEventHandlerRef_t ifgen_le_gpio_AddChangeEventHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Edge_t trigger,
        ///< [IN] Change(s) that should trigger the callback to be called.
        le_gpio_ChangeCallbackFunc_t handlerPtr,
        ///< [IN] The callback function.
        void* contextPtr,
        ///< [IN]
        int32_t sampleMs
        ///< [IN] If not interrupt capable, sample the input this often (ms).
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_gpio_ChangeEvent'
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED void ifgen_le_gpio_RemoveChangeEventHandler
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_ChangeEventHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the edge detection mode. This function can only be used when a handler is registered
 * in order to prevent interrupts being generated and not handled
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_SetEdgeSense
(
    le_msg_SessionRef_t _ifgen_sessionRef,
        le_gpio_Edge_t trigger
        ///< [IN] Change(s) that should trigger the callback to be called.
);

//--------------------------------------------------------------------------------------------------
/**
 * Turn off edge detection
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_result_t ifgen_le_gpio_DisableEdgeSense
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the pin is configured as an output.
 *
 * @return true = output, false = input.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_le_gpio_IsOutput
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the pin is configured as an input.
 *
 * @return true = input, false = output.
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_le_gpio_IsInput
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of edge sensing.
 *
 * @return The current configured value
 *
 * @note it is invalid to read the edge sense of an output
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_gpio_Edge_t ifgen_le_gpio_GetEdgeSense
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value the pin polarity.
 *
 * @return The current configured value
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_gpio_Polarity_t ifgen_le_gpio_GetPolarity
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the pin is currently active.
 *
 * @return true = active, false = inactive.
 *
 * @note this function can only be used on output pins
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED bool ifgen_le_gpio_IsActive
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of pull up and down resistors.
 *
 * @return The current configured value
 */
//--------------------------------------------------------------------------------------------------
LE_SHARED le_gpio_PullUpDown_t ifgen_le_gpio_GetPullUpDown
(
    le_msg_SessionRef_t _ifgen_sessionRef
);

#endif // LE_GPIO_COMMON_H_INCLUDE_GUARD