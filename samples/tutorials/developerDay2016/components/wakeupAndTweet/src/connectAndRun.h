//--------------------------------------------------------------------------------------------------
/**
 * This module provides the ability to execute a function upon successfully establishing a data
 * connection.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//--------------------------------------------------------------------------------------------------
#ifndef CONNECT_AND_RUN_H
#define CONNECT_AND_RUN_H

#include <functional>
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * Establish an internet connection using the data connection service and then run the provided
 * function.
 *
 * @return
 *      - LE_OK if the data connection is attempting to be established
 *      - LE_COMM_ERROR if the data connection request was rejected
 */
//--------------------------------------------------------------------------------------------------
le_result_t wakeupAndTweet_ConnectAndRun
(
    std::function<void()> onConnect  ///< Function to run once the connection is established
);

#endif // CONNECT_AND_RUN_H
