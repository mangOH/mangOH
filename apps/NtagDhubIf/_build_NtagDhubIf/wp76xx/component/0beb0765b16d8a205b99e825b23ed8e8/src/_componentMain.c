/*
 * AUTO-GENERATED _componentMain.c for the NtagDhubIfComponent component.

 * Don't bother hand-editing this file.
 */

#include "legato.h"
#include "../liblegato/eventLoop.h"
#include "../liblegato/log.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char* _NtagDhubIfComponent_le_gpioPin23_ServiceInstanceName;
const char** le_gpioPin23_ServiceInstanceNamePtr = &_NtagDhubIfComponent_le_gpioPin23_ServiceInstanceName;
void le_gpioPin23_ConnectService(void);
// Component log session variables.
le_log_SessionRef_t NtagDhubIfComponent_LogSession;
le_log_Level_t* NtagDhubIfComponent_LogLevelFilterPtr;

// Declare component's COMPONENT_INIT_ONCE function,
// and provide default empty implementation.
__attribute__((weak))
void _NtagDhubIfComponent_COMPONENT_INIT_ONCE(void)
{
}
// Component initialization function (COMPONENT_INIT).
void _NtagDhubIfComponent_COMPONENT_INIT(void);

// Library initialization function.
// Will be called by the dynamic linker loader when the library is loaded.
__attribute__((constructor)) void _NtagDhubIfComponent_Init(void)
{
    LE_DEBUG("Initializing NtagDhubIfComponent component library.");

    // Connect client-side IPC interfaces.
    le_gpioPin23_ConnectService();

    // Register the component with the Log Daemon.
    NtagDhubIfComponent_LogSession = log_RegComponent("NtagDhubIfComponent", &NtagDhubIfComponent_LogLevelFilterPtr);

// Queue the default component's COMPONENT_INIT_ONCE to Event Loop.
    event_QueueComponentInit(_NtagDhubIfComponent_COMPONENT_INIT_ONCE);

    //Queue the COMPONENT_INIT function to be called by the event loop
    event_QueueComponentInit(_NtagDhubIfComponent_COMPONENT_INIT);
}


#ifdef __cplusplus
}
#endif
