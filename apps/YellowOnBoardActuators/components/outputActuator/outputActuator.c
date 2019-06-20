#include "legato.h"
#include "interfaces.h"

#include "outputActuator.h"

//--------------------------------------------------------------------------------------------------
/**
 * Writes a string to a file.
 *
 * @return
 * - LE_OK on success.
 * - LE_FAULT if the file couldn't be opened.
 * - LE_IO_ERROR if the write failed.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t WriteStringToFile
(
    const char *path,   //< null-terminated file system path to write to.
    const char *s       //< null-terminated string to write (null char won't be written).
)
{
    le_result_t res = LE_OK;
    const size_t sLen = strlen(s);
    FILE *f = fopen(path, "r+");
    if (!f)
    {
        LE_ERROR("Couldn't open %s: %s", path, strerror(errno));
        res = LE_FAULT;
        goto done;
    }

    if (fwrite(s, 1, sLen, f) != sLen)
    {
        LE_ERROR("Write to %s failed", path);
        res = LE_IO_ERROR;
        goto cleanup;
    }

cleanup:
    fclose(f);
done:
    return res;
}

//--------------------------------------------------------------------------------------------------
/**
 * Callback that gets called when the Data Hub delivers an update to an output resource that we
 * created.
 */
//--------------------------------------------------------------------------------------------------
static void ActuatorPushHandler
(
    double timestamp,
    bool value,
    void *context   //< null-terminated sysfs path string.
)
{
    const char* sysfsPath = context;

    LE_ASSERT(LE_OK == WriteStringToFile(sysfsPath, value ? "1" : "0"));
}

//--------------------------------------------------------------------------------------------------
/**
 * Register a Boolean actuator, making it appear at a given resource path in the Data Hub and
 * relaying setpoints from that Data Hub resource to a given sysfs file. After calling this
 * function, Boolean updates to the Data Hub output resource at dhubBasePath + "/enable" will
 * result in either a '0' or a '1' being written to the file at sysfsPath.
 *
 * The default value will be False ('0').
 */
//--------------------------------------------------------------------------------------------------
void outputActuator_Register
(
    const char *dhubBasePath,   //< Resource path in Data Hub's namespace for the calling app.
    const char *sysfsPath   //< Path in file system of sysfs file to be controlled from Data Hub.
)
{
    /*
     * NOTE: The current design assumes that once an actuator has been registered, it can't be
     * deregistered.
     */

    // Construct the full Data Hub resource path (on the stack, because we won't need it once we
    // return from this function).
    char dhubOutputPath[DHUB_MAX_RESOURCE_PATH_LEN];
    int len = snprintf(dhubOutputPath, sizeof(dhubOutputPath), "%s/%s", dhubBasePath, "enable");
    if (len >= sizeof(dhubOutputPath))
    {
        LE_FATAL("Data Hub resource path too long (%d > %zu) (%s)",
                 len,
                 sizeof(dhubOutputPath),
                 dhubBasePath);
    }

    // Allocate some heap memory for the sysfs path and save it there. This will be the context
    // for the callbacks from the Data Hub when updates are received for the output resource.
    size_t contextSize = strlen(sysfsPath) + 1;
    char* contextPtr = malloc(contextSize);
    LE_ASSERT(contextPtr);
    LE_ASSERT(LE_OK == le_utf8_Copy(contextPtr, sysfsPath, contextSize, NULL));

    // Create the Data Hub resource, set its default value, and register for callbacks on update.
    le_result_t res;
    const char *noUnits = "";
    res = dhub_CreateOutput(dhubOutputPath, DHUB_DATA_TYPE_BOOLEAN, noUnits);
    if (res != LE_OK)
    {
        LE_FATAL("Failed to create DataHub output %s", dhubOutputPath);
    }
    dhub_SetBooleanDefault(dhubOutputPath, false);
    dhub_AddBooleanPushHandler(dhubOutputPath, ActuatorPushHandler, contextPtr);
}

COMPONENT_INIT
{
}
