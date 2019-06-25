//--------------------------------------------------------------------------------------------------
/**
 * Data Logger that captures sensor data from the various on-board sensors on the mangOH
 * Yellow.  This utilizes the Data Hub's persistent observation buffers to capture a historical
 * log of the samples over time.
 *
 * The on-board flash has about 70 MB available (in /mnt/flash, where the Data Hub persistant
 * backups are kept).  We shouldn't use more than half of this, because the Data Hub uses the
 * Atomic File Access API to do the saving, which requires twice the space for each file.
 *
 * All of the work that this app does is actually done at start-up.  First, it configures the
 * data hub for observing the sensors, then it saves any previously sampled data from the
 * observations and erases those old sensor readings to make room for new samples that may have
 * the same timestamp range as the old samples (because there's no battery for the RTC), and
 * finally, it enables the sensors.
 *
 * Unfortunately, because the Data Hub doesn't have a synchronous version of
 * dhubQuery_ReadBufferJson(), we have to break up the work as follows:
 * - COMPONENT_INIT configures the Data Hub and starts saving the observation buffers to files.
 *   There will be one separate callback to BufferReadComplete() for each observation.
 * - BufferReadComplete() (completion callback from dhubQuery_ReadBufferJson()) closes the file,
 *   erases the observation buffer, and enables the sensor.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"

#define SAMPLES_PER_MINUTE 6
#define MAX_SAMPLES_PER_SENSOR (SAMPLES_PER_MINUTE * 60 /*mins*/ * 24 /*hrs*/ * 5 /*days*/)
#define BACKUP_PERIOD 60 // seconds between buffer backups to flash.

#define SAVE_DIR_PATH "/home/root/save"

//--------------------------------------------------------------------------------------------------
/**
 * Contextual information for one sensor and its observation.
 */
//--------------------------------------------------------------------------------------------------
typedef struct
{
    const char* name;   ///< The name of the sensor, to be used to name the save files and the
                        ///< observation.

    const char* resPath; ///< DataHub path where the sensor's value, period, and enable can be
                         ///< found. (e.g., "/app/light" or "/app/environment/pressure")

    int saveFd;     ///< File descriptor of save file (or -1 if file not open).
}
Sensor_t;


//--------------------------------------------------------------------------------------------------
/**
 * List of sensor information for logged sensors.
 */
//--------------------------------------------------------------------------------------------------
static Sensor_t SensorContextList[] =
{
    // name,        resPath,            saveFd (-1)
    { "light",      "/app/light",       -1 },
    { "battery",    "/app/battery",     -1 },
    { "gyro",       "/app/imu/gyro",    -1 },
    { "accel",      "/app/imu/accel",   -1 },
    { "imuTemp",    "/app/imu/temp",    -1 },
    { "env",        "/app/environment", -1 }
};


//--------------------------------------------------------------------------------------------------
/**
 * Create a save directory whose name is an unsigned integer under the "/save" directory.
 * The lowest unsigned integer that doesn't already exist under /save will be used.
 *
 * @return The integer identifier of the directory.
 */
//--------------------------------------------------------------------------------------------------
static uint MakeSaveDir
(
    void
)
//--------------------------------------------------------------------------------------------------
{
    // Make the parent directory, if it doesn't yet exist.
    LE_ASSERT(le_dir_MakePath(SAVE_DIR_PATH, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == LE_OK);

    char path[128];
    uint dirId;
    le_result_t result;

    // Start at 0 and keep incrementing until we find an integer that doesn't already have
    // a directory with that name.
    for (dirId = 0, result = LE_DUPLICATE; result == LE_DUPLICATE; dirId++)
    {
        LE_ASSERT(sizeof(path) > snprintf(path, sizeof(path), SAVE_DIR_PATH "/%u", dirId));

        result = le_dir_Make(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        if (result == LE_OK)
        {
            return dirId;
        }
    }

    LE_FATAL("Error (%s) creating save directory (%s).", LE_RESULT_TXT(result), path);
}


//--------------------------------------------------------------------------------------------------
/**
 * Use the Data Hub Admin API to configure the monitoring and persistent buffering of a
 * sensor input's samples under an observation.
 */
//--------------------------------------------------------------------------------------------------
static void ConfigureLogging
(
    Sensor_t* sensorPtr
)
//--------------------------------------------------------------------------------------------------
{
    char obsPath[128];
    char valPath[128];

    LE_ASSERT(sizeof(obsPath) > snprintf(obsPath, sizeof(obsPath), "/obs/%s", sensorPtr->name));

    LE_ASSERT(sizeof(valPath) > snprintf(valPath, sizeof(valPath), "%s/value", sensorPtr->resPath));

    dhubAdmin_SetSource(obsPath, valPath);
    dhubAdmin_SetBufferMaxCount(obsPath, MAX_SAMPLES_PER_SENSOR);
    dhubAdmin_SetBufferBackupPeriod(obsPath, BACKUP_PERIOD);
}


//--------------------------------------------------------------------------------------------------
/**
 * Enable the polling of a sensor.
 */
//--------------------------------------------------------------------------------------------------
static void EnableSensor
(
    Sensor_t* sensorPtr
)
//--------------------------------------------------------------------------------------------------
{
    char path[128];

    // Set the period
    LE_ASSERT(sizeof(path) > snprintf(path, sizeof(path), "%s/period", sensorPtr->resPath));
    dhubAdmin_SetNumericOverride(path, 60.0 / SAMPLES_PER_MINUTE);

    // Enable the sensor
    LE_ASSERT(sizeof(path) > snprintf(path, sizeof(path), "%s/enable", sensorPtr->resPath));
    dhubAdmin_SetBooleanOverride(path, true);
}


//--------------------------------------------------------------------------------------------------
/**
 * Erase an observation's buffer of samples.
 */
//--------------------------------------------------------------------------------------------------
static void EraseObservationBuffer
(
    Sensor_t* sensorPtr
)
//--------------------------------------------------------------------------------------------------
{
    // Erase the observation buffer contents by setting the maximum number of samples to zero
    // momentarily.
    uint32_t bufferMaxCount = dhubAdmin_GetBufferMaxCount(sensorPtr->name);
    dhubAdmin_SetBufferMaxCount(sensorPtr->name, 0);
    dhubAdmin_SetBufferMaxCount(sensorPtr->name, bufferMaxCount);
}


//--------------------------------------------------------------------------------------------------
/**
 * Completion call-back for dhubQuery_ReadBufferJson().  This closes the file, erases the
 * observation buffer and enables the sensor to begin the delivery of new samples to the
 * observation.
 */
//--------------------------------------------------------------------------------------------------
static void BufferReadComplete
(
    le_result_t result,
    void* contextPtr
)
//--------------------------------------------------------------------------------------------------
{
    Sensor_t* sensorPtr = contextPtr;

    if (result != LE_OK)
    {
        LE_CRIT("Failed (%s) to read observation buffer (%s) to file.",
                LE_RESULT_TXT(result),
                sensorPtr->name);

        // Continue on, losing old samples. :(
    }

    LE_ASSERT(le_atomFile_Close(sensorPtr->saveFd) == LE_OK);
    LE_INFO("Successfully saved %s samples (to fd %d)", sensorPtr->name, sensorPtr->saveFd);
    sensorPtr->saveFd = -1;

    EraseObservationBuffer(sensorPtr);
    EnableSensor(sensorPtr);
}


//--------------------------------------------------------------------------------------------------
/**
 * Read old samples from an observation into a file.  BufferSaveComplete() will be called when
 * the old samples have all be written to the file.
 *
 * The file will be saved at the path "/save/<dirId>/<name>".
 *
 * @warning It is assumed that the directory "/save/<dirId>" exists before this function is called.
 */
//--------------------------------------------------------------------------------------------------
static void SaveOldSamples
(
    Sensor_t* sensorPtr,
    uint dirId
)
//--------------------------------------------------------------------------------------------------
{
    char path[128];

    LE_ASSERT(sizeof(path) > snprintf(path,
                                      sizeof(path),
                                      SAVE_DIR_PATH "/%u/%s",
                                      dirId,
                                      sensorPtr->name) );

    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
    int fd = le_atomFile_Create(path, LE_FLOCK_WRITE, LE_FLOCK_FAIL_IF_EXIST, mode);
    if (fd < 0)
    {
        LE_CRIT("Failed (%s) to open save file (%s).", LE_RESULT_TXT(fd), path);

        // Limp on, losing the old samples. :(
        EraseObservationBuffer(sensorPtr);
        EnableSensor(sensorPtr);
    }
    else
    {
        LE_INFO("Backing up %s samples to %s", sensorPtr->name, path);

        sensorPtr->saveFd = fd; // Remember the file descriptor so we can close it later.

        // dhubQuery_ReadBufferJson() will take ownership of the fd and pass it across to
        // the Data Hub.  But, we need to use the Atomic File API to close the same fd later,
        // so we pass a duplicate fd to the Data Hub instead.
        fd = dup(fd);
        LE_FATAL_IF(fd < 0, "Failed to dup fd %d (%m)", sensorPtr->saveFd)
        dhubQuery_ReadBufferJson(sensorPtr->name, NAN, fd, BufferReadComplete, sensorPtr);

        // Wait for call-back to BufferReadComplete().
    }
}


COMPONENT_INIT
{
    uint dirId = MakeSaveDir();

    LE_INFO("---- Started Data Logger instance %u ----", dirId);

    uint i;
    for (i = 0; i < NUM_ARRAY_MEMBERS(SensorContextList); i++)
    {
        Sensor_t* sensorPtr = SensorContextList + i;
        ConfigureLogging(sensorPtr);
        SaveOldSamples(sensorPtr, dirId);
    }
}
