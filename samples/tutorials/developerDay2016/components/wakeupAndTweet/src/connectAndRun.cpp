//--------------------------------------------------------------------------------------------------
/**
 * This module provides the ability to execute a function upon successfully establishing a data
 * connection.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//--------------------------------------------------------------------------------------------------

#include "connectAndRun.h"
#include "interfaces.h"
#include "legato.h"

//--------------------------------------------------------------------------------------------------
/**
 * The context data of a connectAndRun call.
 */
//--------------------------------------------------------------------------------------------------
struct ConnectAndRunRecord
{
    le_data_ConnectionStateHandlerRef_t connectionStateHandlerRef;
    le_data_RequestObjRef_t dataConnection;
    std::function<void()> onConnect;
};


//--------------------------------------------------------------------------------------------------
/**
 * Handles data connection state events. The onConnect function stored within the
 * ConnectAndRunRecord is executed when a connection is established.
 */
//--------------------------------------------------------------------------------------------------
static void DataConnectionStateHandler
(
    const char* interfaceName,
    bool isConnected,
    void* contextPtr
)
{
    struct ConnectAndRunRecord* record = static_cast<struct ConnectAndRunRecord*>(contextPtr);
    LE_DEBUG("Data connection state event received. connected=%d", isConnected);
    if (isConnected)
    {
        record->onConnect();

        // Deregister the connection state handler and release the data connection now that the
        // onConnect action has been completed.
        le_data_RemoveConnectionStateHandler(record->connectionStateHandlerRef);
        le_data_Release(record->dataConnection);

        delete record;
    }
}

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
    std::function<void()> onConnect
)
{
    // This record will be deleted by DataConnectionStateHandler once the connection is established
    // and onConnect has been run.
    auto record = new ConnectAndRunRecord();

    record->onConnect = onConnect;
    record->connectionStateHandlerRef =
        le_data_AddConnectionStateHandler(DataConnectionStateHandler, record);
    record->dataConnection = le_data_Request();
    if (record->dataConnection == NULL)
    {
        LE_WARN("Couldn't establish a data connection");
        le_data_RemoveConnectionStateHandler(record->connectionStateHandlerRef);
        delete record;
        return LE_COMM_ERROR;
    }

    return LE_OK;
}
