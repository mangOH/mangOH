#include "legato.h"
#include "interfaces.h"

#include "outputActuator.h"

#define DHUB_PATH_LENGTH 256
#define SYSFS_PATH_LENGTH 256

struct OutputActuator
{
    char dhubOutputPath[DHUB_PATH_LENGTH];
    char sysfsPath[SYSFS_PATH_LENGTH];
    dhub_BooleanPushHandlerRef_t pushHandlerRef;
};

static le_result_t WriteStringToFile(const char *path, const char *s)
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

static void ActuatorPushHandler
(
    double timestamp,
    bool value,
    void *context
)
{
    struct OutputActuator *oa = context;
    le_result_t res = WriteStringToFile(oa->sysfsPath, value ? "1" : "0");
    if (res != LE_OK)
    {
        return;
    }
}

le_result_t RegisterOutputActuator(const char *dhubBasePath, const char *sysfsPath)
{
    /*
     * TODO: The current design assumes that once an actuator has been registered, it can't be
     * deregistered so I make no attempt to hang onto the pointer so that it can be cleaned up
     * later.
     */
    struct OutputActuator *oa = calloc(sizeof(*oa), 1);
    LE_ASSERT(oa);
    le_result_t res = LE_OK;

    strncpy(oa->sysfsPath, sysfsPath, SYSFS_PATH_LENGTH);
    LE_FATAL_IF(oa->sysfsPath[SYSFS_PATH_LENGTH - 1] != '\0', "path too long");

    int snprintfRes = snprintf(oa->dhubOutputPath, sizeof(oa->dhubOutputPath), "%s/%s", dhubBasePath, "value");
    if (snprintfRes < 0)
    {
        LE_ERROR("Failed to format output path");
        res = LE_FAULT;
        goto error_output;
    }
    else if (snprintfRes >= sizeof(oa->dhubOutputPath))
    {
        res = LE_OVERFLOW;
        goto error_output;
    }

    const char *noUnits = "";
    res = dhub_CreateOutput(oa->dhubOutputPath, DHUB_DATA_TYPE_BOOLEAN, noUnits);
    if (res != LE_OK)
    {
        LE_ERROR("Failed to create DataHub output %s", oa->dhubOutputPath);
        goto error_output;
    }
    dhub_SetBooleanDefault(oa->dhubOutputPath, false);

    oa->pushHandlerRef = dhub_AddBooleanPushHandler(oa->dhubOutputPath, ActuatorPushHandler, oa);

    return res;

error_output:
    free(oa);
    return res;
}

COMPONENT_INIT
{
}
