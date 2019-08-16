

/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */

/**
 * @page c_gpio GPIO
 *
 * @ref le_gpio_interface.h "API Reference" <br>
 * @ref sampleApps_gpioCf3 "GPIO sample app"
 *
 * <HR>
 *
 * This API is used by apps to control general-purpose digital input/output pins.
 *
 * A GPIO pin typically has one or more of the following features:
 * - configured as an input pin or an output pin.
 * - have an internal pull-up resistor or pull-down resistor enabled, or neither.
 * - if an output, can be push-pull or open-drain.
 * - if an input, can trigger an @e interrupt (asynchronous notification of state change).
 *
 * Output pins can be driven in three modes:
 * - <b>push-pull</b> one transistor connects to the supply and another transistor connects to
 * ground (only one is operated at a time).
 * - <b>tri-state</b> same as push-pull with an added high-impedance (high Z) state to disconnect
 *    pin from both ground and supply.
 * - <b>open drain</b> transistor connects only to ground. Can only be used to pull low.
 *
 * Pins also have a @e polarity mode:
 * - <b> active-high </b> polarity pin is read/written as a digital 1 (true) when its voltage is
 *  "high" and 0 (false) when its voltage is "low" (grounded).
 * - <b> active-low </b> pin is read/written as a digital 1 (true) when its voltage is
 *  "low" (grounded) and 0 (false) when its voltage is "high".
 *
 * The following functions are used to configure the GPIO pin:
 * - SetInput() - Configure as an input pin.
 * - SetPushPullOutput() - Configure as push-pull output pin (can drive high or low).
 * - SetTriStateOutput() - Configure as tri-state output pin (can drive high or low or neither).
 * - SetOpenDrainOutput() - Configure as open-drain output pin (only pulls low).
 * - EnablePullUp() - Enables the internal pull-up resistor (and disables the pull-down).
 * - EnablePullDown() - Enables the internal pull-down resistor (and disables the pull-up).
 * - DisableResistors() - Disables the internal pull-up/down resistors.
 * - SetEdgeSense() - Set the edge sensing on an input pin (only works if you have an EventHandler).
 *
 * To set the level of an output pin, call Activate(), Deactivate(), or SetHighZ().
 *
 * To poll the value of an input pin, call Read().
 *
 * Use the ChangeEvent to register a notification callback function to be called when the
 * state of an input pin changes. Thje type of edge detection can then be modified by calling
 * SetEdgeSense() or DisableEdgeSense()
 * @note The client will be killed for below scenarios:
 * - Only one handler can be registered per pin. Subsequent attempts to register a handler
 *   will result in the client being killed.
 * - If the GPIO object reference is NULL or not initialized.
 * - When unable to set edge detection correctly.
 *
 * The following functions can be used to read the current setting for a GPIO Pin. In a Linux
 * environment these values are read from the sysfs and reflect the actual value at the time
 * the function is called.
 * - IsOutput() - Is the pin currently an output?
 * - IsInput() - Is the pin currently an input?
 * - IsActive() - Is an output pin currently being driven? (corresponds to the value file in sysfs)
 * - GetPolarity() - Retrieve the current polarity (active-low or active-high)
 * - GetEdgeSense() - What edge sensing has been enabled on an input pin?
 * - GetPullUpDown() - What pull-up or pull-down has been enabled?
 *
 * Each GPIO pin is accessed through a single IPC service.  This makes it possible to use bindings
 * to control which GPIO pins each app is allowed to access.  It also simplifies the API by removing
 * the need to specify which pin is desired and allows the pins to be named differently in the
 * client and the server, so the client can be more portable. Only one client can connect to each
 * pin.
 *
 * @section thread Using the GPIOs in a thread
 *
 * Each GPIO pin can be accessed in a thread. APIs @c le_gpioPinXX_ConnectService or
 * @c le_gpioPinXXTryConnectService need to be called to connect the GPIOXX to the GPIO service
 * APIs in a thread. Normally, the ConnectService is automatically called for the main thread.
 *
 * Once the GPIOXX is used, it cannot be accessed by another thread. When no longer used, it can be
 * relinquished by calling API @c le_gpioPinXX_DisconnectService. The GPIO service for GPIOXX will
 * be disconnected leaving the GPIOXX free to be used by another thread.
 *
 * @warning In order to use the GPIO service for GPIOXX in a thread, API
 * @c le_gpioPinXX_DisconnectService must explicitly be called previously in the main thread.
 *
 * @section bindings Using Bindings
 *
 * To create a binding from your app to pin 22 of the GPIO service,
 * add something like this to your @c .adef binding section:
 *
 * @verbatim
bindings:
{
    ui.frontPanel.powerLed -> gpioService.le_gpio22
}
@endverbatim
 *
 * This connects your component called @c frontPanel to the instance
 * of this API that can be used to drive GPIO 22. Setting the pin high looks like this:
 * @code
 * {
 *     // Configure the output type
 *     powerLed_SetPushPullOutput(LE_GPIOPIN22_ACTIVE_HIGH, false);
 *
 *     // Some time later ... set the GPIO high
 *     powerLed_Activate();
 * }
 * @endcode
 *
 * For details on how the GPIOs exposed by this service map to a CF3 module
 * (like the WP85), see @ref sampleApps_gpioCf3 "GPIO sample app".
 *
 * @section gpioConfig Configuring available GPIOs
 *
 * The GPIOs that are available for use are advertised in the sysfs at
 * @verbatim /sys/class/gpio/gpiochip1/mask @endverbatim
 * For each entry in this bitmask, a service will be advertised to allow use of the pin.
 * However, if a pin needs to be disabled form being accessed, e.g. it carries out some system
 * function that should not be available to apps, then it can be disabled by adding an entry to the
 * config tree.
 *
 * For example, to disable pin 6 from being used, add the entry
 * @verbatim gpioService:/pins/disabled/n @endverbatim
 * where n is the GPIO number. The value should be set to true to disable that service. If the value
 * is either false or absent then the service will run. Entries can be added using the config tool,
 * for example  * @verbatim config set gpioService:/pins/disabled/13 true bool@endverbatim
 * will disable the service for pin 13. Note that specifying the type as bool is vital as the config
 * tool defaults to the string type, and hence any value set will default to false.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
/**
 * @file le_gpio_interface.h
 *
 * Legato @ref c_gpio include file.
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#ifndef LE_GPIOPIN23_INTERFACE_H_INCLUDE_GUARD
#define LE_GPIOPIN23_INTERFACE_H_INCLUDE_GUARD


#include "legato.h"

// Internal includes for this interface
#include "le_gpio_common.h"
//--------------------------------------------------------------------------------------------------
/**
 * Type for handler called when a server disconnects.
 */
//--------------------------------------------------------------------------------------------------
typedef void (*le_gpioPin23_DisconnectHandler_t)(void *);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Connect the current client thread to the service providing this API. Block until the service is
 * available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_gpioPin23_ConnectService
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Try to connect the current client thread to the service providing this API. Return with an error
 * if the service is not available.
 *
 * For each thread that wants to use this API, either ConnectService or TryConnectService must be
 * called before any other functions in this API.  Normally, ConnectService is automatically called
 * for the main thread, but not for any other thread. For details, see @ref apiFilesC_client.
 *
 * This function is created automatically.
 *
 * @return
 *  - LE_OK if the client connected successfully to the service.
 *  - LE_UNAVAILABLE if the server is not currently offering the service to which the client is
 *    bound.
 *  - LE_NOT_PERMITTED if the client interface is not bound to any service (doesn't have a binding).
 *  - LE_COMM_ERROR if the Service Directory cannot be reached.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_TryConnectService
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set handler called when server disconnection is detected.
 *
 * When a server connection is lost, call this handler then exit with LE_FATAL.  If a program wants
 * to continue without exiting, it should call longjmp() from inside the handler.
 */
//--------------------------------------------------------------------------------------------------
LE_FULL_API void le_gpioPin23_SetServerDisconnectHandler
(
    le_gpioPin23_DisconnectHandler_t disconnectHandler,
    void *contextPtr
);

//--------------------------------------------------------------------------------------------------
/**
 *
 * Disconnect the current client thread from the service providing this API.
 *
 * Normally, this function doesn't need to be called. After this function is called, there's no
 * longer a connection to the service, and the functions in this API can't be used. For details, see
 * @ref apiFilesC_client.
 *
 * This function is created automatically.
 */
//--------------------------------------------------------------------------------------------------
void le_gpioPin23_DisconnectService
(
    void
);


//--------------------------------------------------------------------------------------------------
/**
 * Pin polarities.
 */
//--------------------------------------------------------------------------------------------------
typedef le_gpio_Polarity_t le_gpioPin23_Polarity_t;
#define LE_GPIOPIN23_ACTIVE_HIGH   LE_GPIO_ACTIVE_HIGH        ///< GPIO active-high, output is 1
#define LE_GPIOPIN23_ACTIVE_LOW   LE_GPIO_ACTIVE_LOW        ///< GPIO active-low, output is 0


//--------------------------------------------------------------------------------------------------
/**
 * State change event handler (callback).
 */
//--------------------------------------------------------------------------------------------------
typedef le_gpio_ChangeCallbackFunc_t le_gpioPin23_ChangeCallbackFunc_t;


//--------------------------------------------------------------------------------------------------
/**
 * Edge transitions.
 */
//--------------------------------------------------------------------------------------------------
typedef le_gpio_Edge_t le_gpioPin23_Edge_t;
#define LE_GPIOPIN23_EDGE_NONE   LE_GPIO_EDGE_NONE        ///< No edge detection
#define LE_GPIOPIN23_EDGE_RISING   LE_GPIO_EDGE_RISING        ///< Notify when voltage goes from low to high.
#define LE_GPIOPIN23_EDGE_FALLING   LE_GPIO_EDGE_FALLING        ///< Notify when voltage goes from high to low.
#define LE_GPIOPIN23_EDGE_BOTH   LE_GPIO_EDGE_BOTH        ///< Notify when pin voltage changes state in either direction.


//--------------------------------------------------------------------------------------------------
/**
 * Reference type used by Add/Remove functions for EVENT 'le_gpioPin23_ChangeEvent'
 */
//--------------------------------------------------------------------------------------------------
typedef le_gpio_ChangeEventHandlerRef_t le_gpioPin23_ChangeEventHandlerRef_t;


//--------------------------------------------------------------------------------------------------
/**
 * Pull up or down.
 */
//--------------------------------------------------------------------------------------------------
typedef le_gpio_PullUpDown_t le_gpioPin23_PullUpDown_t;
#define LE_GPIOPIN23_PULL_OFF   LE_GPIO_PULL_OFF        ///< Neither resistor is enabled
#define LE_GPIOPIN23_PULL_DOWN   LE_GPIO_PULL_DOWN        ///< Pulldown resistor is enabled
#define LE_GPIOPIN23_PULL_UP   LE_GPIO_PULL_UP        ///< Pullup resistor is enabled


//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as an input pin.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_SetInput
(
    le_gpioPin23_Polarity_t polarity
        ///< [IN] Active-high or active-low.
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as a push-pull output pin.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_SetPushPullOutput
(
    le_gpioPin23_Polarity_t polarity,
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
le_result_t le_gpioPin23_SetTriStateOutput
(
    le_gpioPin23_Polarity_t polarity
        ///< [IN] Active-high or active-low.
);

//--------------------------------------------------------------------------------------------------
/**
 * Configure the pin as an open-drain output pin.  "High" is a high-impedance state, while "Low"
 * pulls the pin to ground.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_SetOpenDrainOutput
(
    le_gpioPin23_Polarity_t polarity,
        ///< [IN] Active-high or active-low.
    bool value
        ///< [IN] Initial value to drive (true = active, false = inactive)
);

//--------------------------------------------------------------------------------------------------
/**
 * Enable the pull-up resistor (disables pull-down if previously enabled).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_EnablePullUp
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Enable the pull-down resistor (disables pull-up if previously enabled).
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_EnablePullDown
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Disable the pull-up and pull-down resistors.  Does nothing if both are already disabled.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_DisableResistors
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to active state.
 *
 * @warning Only valid for output pins.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_Activate
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to inactive state.
 *
 * @warning Only valid for output pins.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_Deactivate
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Set output pin to high impedance state.
 *
 * @warning Only valid for tri-state or open-drain output pins.
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_SetHighZ
(
    void
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
bool le_gpioPin23_Read
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Add handler function for EVENT 'le_gpioPin23_ChangeEvent'
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
le_gpioPin23_ChangeEventHandlerRef_t le_gpioPin23_AddChangeEventHandler
(
    le_gpioPin23_Edge_t trigger,
        ///< [IN] Change(s) that should trigger the callback to be called.
    le_gpioPin23_ChangeCallbackFunc_t handlerPtr,
        ///< [IN] The callback function.
    void* contextPtr,
        ///< [IN]
    int32_t sampleMs
        ///< [IN] If not interrupt capable, sample the input this often (ms).
);

//--------------------------------------------------------------------------------------------------
/**
 * Remove handler function for EVENT 'le_gpioPin23_ChangeEvent'
 */
//--------------------------------------------------------------------------------------------------
void le_gpioPin23_RemoveChangeEventHandler
(
    le_gpioPin23_ChangeEventHandlerRef_t handlerRef
        ///< [IN]
);

//--------------------------------------------------------------------------------------------------
/**
 * Set the edge detection mode. This function can only be used when a handler is registered
 * in order to prevent interrupts being generated and not handled
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_SetEdgeSense
(
    le_gpioPin23_Edge_t trigger
        ///< [IN] Change(s) that should trigger the callback to be called.
);

//--------------------------------------------------------------------------------------------------
/**
 * Turn off edge detection
 */
//--------------------------------------------------------------------------------------------------
le_result_t le_gpioPin23_DisableEdgeSense
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the pin is configured as an output.
 *
 * @return true = output, false = input.
 */
//--------------------------------------------------------------------------------------------------
bool le_gpioPin23_IsOutput
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Check if the pin is configured as an input.
 *
 * @return true = input, false = output.
 */
//--------------------------------------------------------------------------------------------------
bool le_gpioPin23_IsInput
(
    void
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
le_gpioPin23_Edge_t le_gpioPin23_GetEdgeSense
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value the pin polarity.
 *
 * @return The current configured value
 */
//--------------------------------------------------------------------------------------------------
le_gpioPin23_Polarity_t le_gpioPin23_GetPolarity
(
    void
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
bool le_gpioPin23_IsActive
(
    void
);

//--------------------------------------------------------------------------------------------------
/**
 * Get the current value of pull up and down resistors.
 *
 * @return The current configured value
 */
//--------------------------------------------------------------------------------------------------
le_gpioPin23_PullUpDown_t le_gpioPin23_GetPullUpDown
(
    void
);

#endif // LE_GPIOPIN23_INTERFACE_H_INCLUDE_GUARD