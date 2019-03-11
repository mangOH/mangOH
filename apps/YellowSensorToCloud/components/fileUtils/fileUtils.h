//--------------------------------------------------------------------------------------------------
/**
 * @file fileUtils.h
 *
 * Utility functions used to read numbers from sysfs files.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#ifndef FILE_UTILS_H_INCLUDE_GUARD
#define FILE_UTILS_H_INCLUDE_GUARD


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
LE_SHARED le_result_t file_ReadInt
(
    const char *filePath,
    int *value
);


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
LE_SHARED le_result_t file_ReadDouble
(
    const char *filePath,
    double *value
);


#endif // FILE_UTILS_H_INCLUDE_GUARD
