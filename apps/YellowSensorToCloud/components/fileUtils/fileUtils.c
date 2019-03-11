//--------------------------------------------------------------------------------------------------
/**
 * @file fileUtils.c
 *
 * Utility functions used to read numbers from sysfs files.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "fileUtils.h"

//--------------------------------------------------------------------------------------------------
/**
 * Read a signed integer from a sysfs file (convert the string contents to a number).
 *
 * @return
 *  - LE_OK if successful
 *  - LE_IO_ERROR if the file could not be opened.
 *  - LE_FORMAT_ERROR if the file contents could not be converted into a signed integer.
 */
//--------------------------------------------------------------------------------------------------
le_result_t file_ReadInt
(
    const char *filePath,
    int *value
)
{
    le_result_t r = LE_OK;
    FILE *f = fopen(filePath, "r");
    if (f == NULL)
    {
        LE_WARN("Couldn't open '%s' - %m", filePath);
        r = LE_IO_ERROR;
        goto done;
    }

    int numScanned = fscanf(f, "%d", value);
    if (numScanned != 1)
    {
        r = LE_FORMAT_ERROR;
    }

    fclose(f);
done:
    return r;
}


//--------------------------------------------------------------------------------------------------
/**
 * Read a floating point number from a sysfs file (convert the string contents to a number).
 *
 * @return
 *  - LE_OK if successful
 *  - LE_IO_ERROR if the file could not be opened.
 *  - LE_FORMAT_ERROR if the file contents could not be converted into a number.
 */
//--------------------------------------------------------------------------------------------------
le_result_t file_ReadDouble
(
    const char *filePath,
    double *value
)
{
    le_result_t r = LE_OK;
    FILE *f = fopen(filePath, "r");
    if (f == NULL)
    {
        LE_WARN("Couldn't open '%s' - %m", filePath);
        r = LE_IO_ERROR;
        goto done;
    }

    int numScanned = fscanf(f, "%lf", value);
    if (numScanned != 1)
    {
        r = LE_FORMAT_ERROR;
    }

    fclose(f);
done:
    return r;
}


COMPONENT_INIT
{
}
