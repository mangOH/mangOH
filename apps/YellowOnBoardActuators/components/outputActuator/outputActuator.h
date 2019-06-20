//--------------------------------------------------------------------------------------------------
/**
 * Boolean output actuator utility.  Does the bulk of the work to register a Boolean actuator in
 * the Data Hub and translates updates from the Data Hub into a '0' or '1' written to a sysfs file.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------
#ifndef _OUTPUT_ACTUATOR_H_
#define _OUTPUT_ACTUATOR_H_

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
LE_SHARED void outputActuator_Register
(
    const char *dhubBasePath,   //< Resource path in Data Hub's namespace for the calling app.
    const char *sysfsPath   //< Path in file system of sysfs file to be controlled from Data Hub.
);

#endif // _OUTPUT_ACTUATOR_H_
