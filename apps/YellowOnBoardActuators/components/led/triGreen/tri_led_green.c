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

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(array) ((unsigned long) (sizeof (array) / sizeof ((array)[0])))
#endif
#define RES_PATH_TRI_GREEN_LED   "tri_led_green"
#define RES_PATH_GREEN_VALUE "tri_led_green/value"

static const char TriGreenLedPath[]   = "/sys/devices/platform/expander.0/tri_led_grn";

typedef struct
{
    const char                         *path;
    dhubIO_DataType_t                  dataType;
    const char                         *units;
    dhubIO_BooleanPushHandlerRef_t     ref;
}
actuator_DhOut_t;

static actuator_DhOut_t dhubResources[] = {
   {RES_PATH_TRI_GREEN_LED,  DHUBIO_DATA_TYPE_BOOLEAN, "", NULL },
};
static void actuator_DhOutputHandler
(
    double  timestamp,
    bool    value,
    void   *context
)
{
    const char *writeData = value ? "1" : "0";
    FILE *ledFile = fopen(TriGreenLedPath, "r+");
    if (ledFile == NULL)
    {
        LE_ERROR("Open LED device file('%s') failed(%d)", TriGreenLedPath, errno);
        return;
    }

    LE_DEBUG("Turn %s LED", value ? "on" : "off");
    if (fwrite(writeData, sizeof(writeData), 1, ledFile) != 1)
    {
        LE_ERROR("Write LED device file('%s') failed", TriGreenLedPath);
    }
    dhubIO_PushBoolean(RES_PATH_GREEN_VALUE, DHUBIO_NOW, value);
    dhubIO_MarkOptional(RES_PATH_TRI_GREEN_LED);
    dhubIO_SetBooleanDefault(RES_PATH_TRI_GREEN_LED,false);
    fclose(ledFile);
     
}







static le_result_t actuator_DhRegister
(
    void
)
{
    le_result_t result = LE_OK;


    for (int i = 0; i < ARRAY_SIZE(dhubResources); i++)
    {
        result = dhubIO_CreateOutput(dhubResources[i].path, 
                                 dhubResources[i].dataType, 
                                 dhubResources[i].units);
        if (LE_OK != result)
        {
            LE_ERROR("Failed to create output resource %s", dhubResources[i].path);
            break;
        }

/*       LE_ASSERT(LE_OK == dhubIO_CreateInput(dhubResources[i].value, DHUBIO_DATA_TYPE_BOOLEAN, ""));
        
         dhubResources[i].ref = dhubIO_AddBooleanPushHandler(dhubResources[i].path, 
                                                        actuator_DhOutputHandler, 
                                                        (void *) &dhubResources[i].gpio_path);
*/
       LE_ASSERT(LE_OK == dhubIO_CreateInput(RES_PATH_GREEN_VALUE, DHUBIO_DATA_TYPE_BOOLEAN, ""));
       dhubResources[i].ref = dhubIO_AddBooleanPushHandler(dhubResources[i].path, actuator_DhOutputHandler,
                                                           NULL );
       

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
static void actuator_SigTermHandler
(
    int pSigNum
)
{
    (void)pSigNum;
    LE_INFO("Remove LED resource");

    dhubIO_DeleteResource(TriGreenLedPath);

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
    le_sig_SetEventHandler(SIGTERM, actuator_SigTermHandler);
    LE_ASSERT(LE_OK ==    actuator_DhRegister());
}

