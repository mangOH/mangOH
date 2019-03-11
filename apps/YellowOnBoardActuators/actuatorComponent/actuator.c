/**
 *
 * This file provides the implementation of @ref c_led
 *
 *
 * <hr>
 *
 * Copyright (C) Sierra Wireless Inc.
 */

#include "legato.h"
#include "interfaces.h"

static const char GenericLedPath[]  = "/sys/devices/platform/expander.0/generic_led";

typedef struct
{
    char         *name;
    le_result_t (*activate)(void);
    le_result_t (*deactivate)(void);
}
actuator_gpio_t;

static actuator_gpio_t gpio[] = {
    { "Generic_Led", generic_led_Activate, generic_led__Deactivate },
    };


typedef struct
{
    const char                 *path;
    io_DataType_t               dataType;
    const char                 *units;
    io_BooleanPushHandlerRef_t  ref;
}
actuator_DhOut_t;

static actuator_DhOut_t dhubResources[] = {
   GenericLedPath , DHUBIO_DATA_TYPE_BOOLEAN, "", NULL },
};


static void actuator_DhOutputHandler
(
    double  timestamp,
    bool    value,
    void   *context
)
{
    actuator_gpio_t *output = (actuator_gpio_t *)context;


    if (output)
    {
        le_result_t result = (true == value) ? output->activate() : output->deactivate();
        LE_INFO("%s : %s (ts: %lf) result: %d", 
                output->name, true == value ? "set" : "clr", timestamp, result);
    }
}


static le_result_t actuator_DhRegister
(
    void
)
{
    le_result_t result = LE_OK;


    for (int i = 0; i < ARRAY_SIZE(dhubResources); i++)
    {
        result = io_CreateOutput(dhubResources[i].path, 
                                 dhubResources[i].dataType, 
                                 dhubResources[i].units);
        if (LE_OK != result)
        {
            LE_ERROR("Failed to create output resource %s", dhubResources[i].path);
            break;
        }

        dhubResources[i].ref = io_AddBooleanPushHandler(dhubResources[i].path, 
                                                        actuator_DhOutputHandler, 
                                                        (void *)&gpio[i]);
        if (NULL == dhubResources[i].ref)
        {
            LE_ERROR("Failed to add handler for output resource %s", dhubResources[i].path);
            result = LE_FAULT;
            break;
        }
    }

    return result;
}


//--------------------------------------------------------------------------------------------------
/**
 * SIGTERM handler to cleanly shutdown
 */
//--------------------------------------------------------------------------------------------------
static void ioServ_SigTermHandler
(
    int pSigNum
)
{
    (void)pSigNum;
    LE_INFO("Remove LED resource");

    io_DeleteResource(GenericLedPath);

}


//--------------------------------------------------------------------------------------------------
/**
 * Main program
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    ///< Catch application termination and shutdown cleanly
    le_sig_Block(SIGTERM);
    le_sig_SetEventHandler(SIGTERM, ioServ_SigTermHandler);
    actuator_DhRegister();

}

