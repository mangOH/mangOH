//--------------------------------------------------------------------------------------------------
/** 
 * This sample app reads state of  IoT1_GPIO1 (gpio25) or Push Button SW200
 * If state change is detected, it makes corresponding change in state of LED D750
 * Use this sample to understand how to configure a gpio as an input or output
 * and use call back function
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
/**
 * Sets default configuration LED D750 as on
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureLed
(
    void
)
{
    // Configure initial value as LED on
   
    LE_FATAL_IF(mangoh_ledGpio_SetPushPullOutput(MANGOH_LEDGPIO_ACTIVE_HIGH, true) != LE_OK,
                "Couldn't configure LED PLAY as a push pull output");
}

//--------------------------------------------------------------------------------------------------
/**
 * Performs initial configuration of the push button (SW200)
 */
//--------------------------------------------------------------------------------------------------
static void ConfigurePushButton
(
    void
)
{
    // Configure Push Button as input and set its initial value as high

    LE_FATAL_IF(mangoh_pushButton_SetInput(MANGOH_PUSHBUTTON_ACTIVE_HIGH ) != LE_OK,
                "Couldn't configure touch sensor as default input high");
}

//--------------------------------------------------------------------------------------------------
/**
 * LED D750 changes state when Push Button changes state
 */
//--------------------------------------------------------------------------------------------------
static  void touch_ledGpio_ChangeHandler
(
    bool state, void *ctx
)
{
    // set call back for change in state of GPIO
    if (state)
    {
        mangoh_ledGpio_Activate();
    }
    else
    {
        mangoh_ledGpio_Deactivate();
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Main program starts here
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{

    LE_INFO("===============touchsensor application has started");

    ConfigureLed();
    ConfigurePushButton();

    mangoh_pushButton_AddChangeEventHandler(MANGOH_PUSHBUTTON_EDGE_BOTH,
                                            touch_ledGpio_ChangeHandler,
                                            NULL,
                                            0);
}
