#include "legato.h"
#include "interfaces.h"

#include <list>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include <jansson.h>

#include "CombainRequestBuilder.h"
#include "CombainResult.h"
#include "CombainHttp.h"
#include "ThreadSafeQueue.h"

#define RES_PATH_API_KEY        "ApiKey/value"

static bool combainApiKeySet = false;
char combainApiKey[MAX_LEN_API_KEY];


struct RequestRecord
{
    ma_combainLocation_LocReqHandleRef_t handle;
    le_msg_SessionRef_t clientSession;
    std::shared_ptr<CombainRequestBuilder> request;
    ma_combainLocation_LocationResultHandlerFunc_t responseHandler;
    void *responseHandlerContext;
    std::shared_ptr<CombainResult> result;
};

// Just use a list for all of the requests because it's very unlikely that there will be more than
// one or two active at a time.
static std::list<RequestRecord> Requests;

ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> RequestJson;
ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> ResponseJson;
le_event_Id_t ResponseAvailableEvent;

static ma_combainLocation_LocReqHandleRef_t GenerateHandle(void);
static RequestRecord* GetRequestRecordFromHandle(
    ma_combainLocation_LocReqHandleRef_t handle, bool matchClientSession);
static bool TryParseAsSuccess(json_t *responseJson, std::shared_ptr<CombainResult>& result);
static bool TryParseAsError(json_t *responseJson, std::shared_ptr<CombainResult>& result);



ma_combainLocation_LocReqHandleRef_t ma_combainLocation_CreateLocationRequest
(
    void
)
{
    Requests.emplace_back();
    auto& r = Requests.back();
    r.handle = GenerateHandle();
    r.clientSession = ma_combainLocation_GetClientSessionRef();
    r.request.reset(new CombainRequestBuilder());

    return r.handle;
}

le_result_t ma_combainLocation_AppendWifiAccessPoint
(
    ma_combainLocation_LocReqHandleRef_t handle,
    const uint8_t *bssid,
    size_t bssidLen,
    const uint8_t *ssid,
    size_t ssidLen,
    int16_t signalStrength
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
	LE_ERROR("Failed to GetRequestRecordFromHandle");
        return LE_BAD_PARAMETER;
    }

    if (!requestRecord->request)
    {
        // Request builder doesn't exist, must have already been submitted
	LE_ERROR("Request builder doesn't exist, must have already been submitted");
        return LE_BUSY;
    }

    std::unique_ptr<WifiApScanItem> ap;
    try {
        ap.reset(new WifiApScanItem(bssid, bssidLen, ssid, ssidLen, signalStrength));
    }
    catch (std::runtime_error& e)
    {
        LE_ERROR("Failed to append AP info: %s", e.what());
        return LE_BAD_PARAMETER;
    }
    requestRecord->request->appendWifiAccessPoint(*ap);

    return LE_OK;
}

le_result_t ma_combainLocation_AppendCellTower
(
    ma_combainLocation_LocReqHandleRef_t handle,
    ma_combainLocation_CellularTech_t cellularTechnology,
    uint16_t mcc,
    uint16_t mnc,           ///< Use systemId, (sid) for CDMA
    uint32_t lac,           ///< Network id for CDMA
    uint32_t cellId,        ///< Basestation Id for CDMAj
    int32_t signalStrength  ///< Signal strength in dBm
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    if (!requestRecord->request)
    {
        // Request builder doesn't exist, must have already been submitted
        return LE_BUSY;
    }

    // TODO

    return LE_OK;
}

le_result_t ma_combainLocation_SubmitLocationRequest
(
    ma_combainLocation_LocReqHandleRef_t handle,
    ma_combainLocation_LocationResultHandlerFunc_t responseHandler,
    void *context
)
{
    /* WLAN caching has so much variance, ssids coming & going, ordered list, ...
     * Need to rethink the algorithm for caching
     * Lets cache the value so that we don't make multiple Combain requests
    static std::string cached_requestBody; */

    if (!combainApiKeySet)
    {
        return LE_UNAVAILABLE;
    }

    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    if (!requestRecord->request)
    {
        // Request builder doesn't exist, must have already been submitted
        return LE_BUSY;
    }

    std::string requestBody = requestRecord->request->generateRequestBody();

    /* if (!cached_requestBody.empty() {
    	LE_INFO("Submitting request: %s", requestBody.c_str());
    	LE_INFO("Cached request: %s", cached_requestBody.c_str());
	if (cached_requestBody.compare(requestBody) == 0) {
	     LE_INFO("cached_requestBody same as requestBody");
	     return LE_DUPLICATE;
	}
    } */

    {
        FILE* f = fopen("request.txt", "w");
        LE_ASSERT(f != NULL);
        fwrite(requestBody.c_str(), 1, requestBody.size(), f);
        fclose(f);
    }
    requestRecord->responseHandler = responseHandler;
    /* cached_requestBody = requestBody; */
	
    requestRecord->responseHandlerContext = context;
    // NULL out the request generator since we're done with it
    requestRecord->request.reset();
    RequestJson.enqueue(std::make_tuple(handle, requestBody));

    return LE_OK;
}

void ma_combainLocation_DestroyLocationRequest
(
    ma_combainLocation_LocReqHandleRef_t handle
)
{
    std::remove_if(
        Requests.begin(),
        Requests.end(),
        [handle] (const RequestRecord& rec) {
            return handle == rec.handle &&
                rec.clientSession == ma_combainLocation_GetClientSessionRef();
        });
}

le_result_t ma_combainLocation_GetSuccessResponse
(
    ma_combainLocation_LocReqHandleRef_t handle,
    double *latitude,
    double *longitude,
    double *accuracyInMeters
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    auto r = requestRecord->result;
    if (!r)
    {
        return LE_UNAVAILABLE;
    }

    if (r->getType() != MA_COMBAINLOCATION_RESULT_SUCCESS)
    {
        return LE_UNAVAILABLE;
    }

    std::shared_ptr<CombainSuccessResponse> sr = std::static_pointer_cast<CombainSuccessResponse>(r);

    *latitude = sr->latitude;
    *longitude = sr->longitude;
    *accuracyInMeters = sr->accuracyInMeters;

    ma_combainLocation_DestroyLocationRequest(handle);

    return LE_OK;
}

le_result_t ma_combainLocation_GetErrorResponse
(
    ma_combainLocation_LocReqHandleRef_t handle,
    char *firstDomain,
    size_t firstDomainLen,
    char *firstReason,
    size_t firstReasonLen,
    char *firstMessage,
    size_t firstMessageLen,
    uint16_t *code,
    char *message,
    size_t messageLen
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    auto r = requestRecord->result;
    if (!r)
    {
        return LE_UNAVAILABLE;
    }

    if (r->getType() != MA_COMBAINLOCATION_RESULT_ERROR)
    {
        return LE_UNAVAILABLE;
    }

    std::shared_ptr<CombainErrorResponse> er = std::static_pointer_cast<CombainErrorResponse>(r);

    *code = er->code;
    strncpy(message, er->message.c_str(), messageLen - 1);
    message[messageLen - 1] = '\0';

    auto it = er->errors.begin();
    if (it != er->errors.end())
    {
        strncpy(firstDomain, it->domain.c_str(), firstDomainLen - 1);
        firstDomain[firstDomainLen - 1] = '\0';

        strncpy(firstReason, it->reason.c_str(), firstReasonLen - 1);
        firstReason[firstReasonLen - 1] = '\0';

        strncpy(firstMessage, it->message.c_str(), firstMessageLen - 1);
        firstMessage[firstMessageLen - 1] = '\0';
    }

    ma_combainLocation_DestroyLocationRequest(handle);

    return LE_OK;
}


le_result_t ma_combainLocation_GetParseFailureResult
(
    ma_combainLocation_LocReqHandleRef_t handle,
    char *unparsedResponse,
    size_t unparsedResponseLen
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, true);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    auto r = requestRecord->result;
    if (!r)
    {
        return LE_UNAVAILABLE;
    }

    if (r->getType() != MA_COMBAINLOCATION_RESULT_RESPONSE_PARSE_FAILURE)
    {
        return LE_UNAVAILABLE;
    }

    std::shared_ptr<CombainResponseParseFailure> rpf =
        std::static_pointer_cast<CombainResponseParseFailure>(r);

    strncpy(
        unparsedResponse,
        rpf->unparsed.c_str(),
        std::min(rpf->unparsed.length() + 1, unparsedResponseLen + 1));

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * A handler for client disconnects which frees all resources associated with the client.
 */
//--------------------------------------------------------------------------------------------------
static void ClientSessionClosedHandler
(
    le_msg_SessionRef_t clientSession,
    void* context
)
{
    std::remove_if(
        Requests.begin(),
        Requests.end(),
        [clientSession] (const RequestRecord& rec) {
            return rec.clientSession == clientSession;
        });
}

static ma_combainLocation_LocReqHandleRef_t GenerateHandle(void)
{
    static uint32_t next = 1;

    ma_combainLocation_LocReqHandleRef_t h = (ma_combainLocation_LocReqHandleRef_t)next;
    next += 2;
    return h;
}

static RequestRecord* GetRequestRecordFromHandle(
    ma_combainLocation_LocReqHandleRef_t handle, bool matchClientSession)
{
    auto it = std::find_if(
        Requests.begin(),
        Requests.end(),
        [handle, matchClientSession] (const RequestRecord& r) -> bool {
            return (
                r.handle == handle && (
                    !matchClientSession ||
                    r.clientSession == ma_combainLocation_GetClientSessionRef()));
        });
    return (it == Requests.end()) ? NULL : &(*it);
}

static void HandleResponseAvailable(void *reportPayload)
{
    auto t = ResponseJson.dequeue();
    ma_combainLocation_LocReqHandleRef_t handle = std::get<0>(t);
    std::string responseJsonStr = std::get<1>(t);

    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle, false);
    if (!requestRecord)
    {
        // Just do nothing the request no longer exists
        LE_WARN("Received a response for an invalid handle");
        return;
    }

    // There should never be a previous result
    //LE_ASSERT(!requestRecord->result);

    // TODO: This is a bit gross that we're using an empty response to signal a communication
    // failure. We may wish to be more expressive about why the communication failed.
    if (responseJsonStr.empty())
    {
        requestRecord->result.reset(new CombainCommunicationFailure());
    }
    else
    {
        // try to parse the response as json
        json_error_t loadError;
        const size_t loadFlags = 0;
        json_t *responseJson = json_loads(responseJsonStr.c_str(), loadFlags, &loadError);
        if (responseJson == NULL)
        {
            requestRecord->result.reset(new CombainResponseParseFailure(responseJsonStr));
        }
        else if (!TryParseAsSuccess(responseJson, requestRecord->result) &&
                 !TryParseAsError(responseJson, requestRecord->result))
        {
            requestRecord->result.reset(new CombainResponseParseFailure(responseJsonStr));
        }

        json_decref(responseJson);
    }

    requestRecord->responseHandler(
        handle, requestRecord->result->getType(), requestRecord->responseHandlerContext);
}

static bool TryParseAsError(json_t *responseJson, std::shared_ptr<CombainResult>& result)
{
    const char *domain;
    const char *reason;
    const char *errorMessage;
    int code;
    const char *message;
    const int errorUnpackRes = json_unpack(
        responseJson,
        "{s:{s:{s:s,s:s,s:s},s:i,s:s}}",
        "error",
        "errors",
        "domain",
        &domain,
        "reason",
        &reason,
        "message",
        &errorMessage,
        "code",
        &code,
        "message",
        &message);
    const bool parseSuccess = (errorUnpackRes == 0);
    if (parseSuccess)
    {
        result.reset(new CombainErrorResponse(code, message, {{domain, reason, errorMessage}}));
    }

    return parseSuccess;
}

static bool TryParseAsSuccess(json_t *responseJson, std::shared_ptr<CombainResult>& result)
{
    double latitude;
    double longitude;
    int accuracy;
    const int successUnpackRes = json_unpack(
        responseJson,
        "{s:{s:F,s:F},s:i}",
        "location",
        "lat",
        &latitude,
        "lng",
        &longitude,
        "accuracy",
        &accuracy);
    const bool parseSuccess = (successUnpackRes == 0);
    if (parseSuccess)
    {
        result.reset(new CombainSuccessResponse(latitude, longitude, accuracy));
    }

    return parseSuccess;
}

bool ma_combainLocation_ServiceAvailable(void)
{
    return combainApiKeySet;
}

static void SetApiKey(double timestamp, const char *api_key, void *contextPtr)
{
    LE_INFO("DHUB SetApiKey called with ApiKey: %s", api_key);
    if (strlen(api_key) > (MAX_LEN_API_KEY - 1))
    {
        LE_INFO("Too large an ApiKey!!");
        return;
    }
    strncpy(combainApiKey, api_key, MAX_LEN_API_KEY - 1);
    combainApiKeySet = strlen(api_key) > 0;
}

COMPONENT_INIT
{
    ResponseAvailableEvent = le_event_CreateId("CombainResponseAvailable", 0);
    le_event_AddHandler(
        "CombainResponseAvailableHandler", ResponseAvailableEvent, HandleResponseAvailable);
    // Register a handler to be notified when clients disconnect
    le_msg_AddServiceCloseHandler(
        ma_combainLocation_GetServiceRef(), ClientSessionClosedHandler, NULL);

    // Let's either get the API key from the config tree or wait for it from dhub
    le_cfg_ConnectService();
    const le_result_t cfgRes = le_cfg_QuickGetString(
        "/ApiKey", combainApiKey, sizeof(combainApiKey) - 1, "");
    if (cfgRes == LE_OK && combainApiKey[0] != '\0')
    {
        LE_INFO("config get of combainApiKey worked");
        combainApiKeySet = true;
    }

    // Let's wait for dhub to set the combainApiKey
    else
    {
    	LE_INFO("Wait for dhub to set the combainApiKey");
    	LE_ASSERT(LE_OK == dhubIO_CreateOutput(RES_PATH_API_KEY, DHUBIO_DATA_TYPE_STRING, ""));
    	dhubIO_AddStringPushHandler(RES_PATH_API_KEY, SetApiKey, NULL);
    	dhubIO_MarkOptional(RES_PATH_API_KEY);
    }

    CombainHttpInit(&RequestJson, &ResponseJson, ResponseAvailableEvent);
    le_thread_Ref_t httpThread = le_thread_Create("CombainHttp", CombainHttpThreadFunc, NULL);
    le_thread_Start(httpThread);
}
