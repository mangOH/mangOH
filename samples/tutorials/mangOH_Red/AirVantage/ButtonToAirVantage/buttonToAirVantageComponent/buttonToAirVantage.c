/**
 * @file
 *
 * Publishes the push button state to AirVantage.
 *
 * <HR>
 *
 * Copyright (C) Sierra Wireless, Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"

static const char PushButtonResource[] = "/push_button";
static le_avdata_RequestSessionObjRef_t AvSession;
static le_avdata_SessionStateHandlerRef_t SessionStateHandlerRef;


static void PushCallbackHandler
(
    le_avdata_PushStatus_t status, ///< push status
    void *context                  ///< Unused context pointer
)
{
    switch (status)
    {
    case LE_AVDATA_PUSH_SUCCESS:
        break;

    case LE_AVDATA_PUSH_FAILED:
        LE_WARN("Failed to push push_button state");
        break;

    default:
        LE_FATAL("Unexpected push status");
        break;
    }
}


//--------------------------------------------------------------------------------------------------
/**
 * Publish the push button state to AirVantage as a boolean.
 */
//--------------------------------------------------------------------------------------------------
static void PushButtonHandler
(
    bool state, ///< true if the button is pressed
    void *ctx   ///< context pointer - not used
)
{
    if (state)
    {
        LE_DEBUG("Button was pressed");
    }
    else
    {
        LE_DEBUG("Button was released");
    }

    le_result_t res = res = le_avdata_SetBool(PushButtonResource, state);
    if (res != LE_OK)
    {
        LE_WARN("Failed to set push_button resource");
    }
    else
    {
        le_avdata_Push(PushButtonResource, PushCallbackHandler, NULL);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Configure the push button input
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureGpios(void)
{
    // Set the push-button GPIO as input
    LE_FATAL_IF(
        mangoh_button_SetInput(MANGOH_BUTTON_ACTIVE_LOW) != LE_OK,
        "Couldn't configure push button as input");
    mangoh_button_AddChangeEventHandler(MANGOH_BUTTON_EDGE_BOTH, PushButtonHandler, NULL, 0);
}

static void AvSessionStateHandler
(
    le_avdata_SessionState_t state,
    void *context
    )
{
    switch (state)
    {
    case LE_AVDATA_SESSION_STARTED:
    {
        le_result_t res = le_avdata_CreateResource(PushButtonResource, LE_AVDATA_ACCESS_VARIABLE);
        if (res != LE_OK && res != LE_DUPLICATE)
        {
            LE_FATAL("Couldn't create push_button variable resource");
        }
        break;
    }

    case LE_AVDATA_SESSION_STOPPED:
        LE_INFO("AVData session stopped.  Push will fail.");
        break;

    default:
        LE_FATAL("Unsupported AV session state %d", state);
        break;
    }
}

COMPONENT_INIT
{
    ConfigureGpios();

    SessionStateHandlerRef = le_avdata_AddSessionStateHandler(AvSessionStateHandler, NULL);
    AvSession = le_avdata_RequestSession();
    LE_FATAL_IF(AvSession == NULL, "Failed to request avdata session");
}
