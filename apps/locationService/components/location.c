//--------------------------------------------------------------------------------------------------
/**
 * Implementation of the Brooklyn position sensor interface to the Data Hub.
 *
 * Copyright (C) Sierra Wireless Inc.
 */
//--------------------------------------------------------------------------------------------------

#include "legato.h"
#include "interfaces.h"
#include "periodicSensor.h"
#include <stdio.h>
#include <time.h>

#define HACCURACY_GOOD	 		50
#define HACCURACY_GREY_ZONE_UPPER	500

typedef enum {GPS, WIFI} Loc_t;

typedef struct ScanReading
{
    double lat;
    double lon;
    double hAccuracy;
    double alt;
    double vAccuracy;
} Scan_t ;

// Hacking - assuming mangOH Yellow and the Cypress chip as wlan1 - TODO: add TI Wifi on wlan0?
//static const char *interfacePtr = "wlan1";

static struct
{
    ma_combainLocation_LocReqHandleRef_t combainHandle;
    bool waitingForWifiResults;
    le_wifiClient_NewEventHandlerRef_t wifiHandler;
} State;

static psensor_Ref_t saved_ref= NULL;

uint64_t GetCurrentTimestamp(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t utcMilliSec = (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000;
    return utcMilliSec;
}

static bool MacAddrStringToBinary(const char* s, uint8_t *b)
{
    size_t inputOffset = 0;
    const size_t macStrLen = (6 * 2) + (6 - 1);
    while (inputOffset < macStrLen)
    {
        if (inputOffset % 3 == 2)
        {
            if (s[inputOffset] != ':')
            {
                return false;
            }
        }
        else
        {
            uint8_t nibble;
            if (s[inputOffset] >= '0' && s[inputOffset] <= '9')
            {
                nibble = s[inputOffset] - '0';
            }
            else if (s[inputOffset] >= 'a' && s[inputOffset] <= 'f')
            {
                nibble = 10 + s[inputOffset] - 'a';
            }
            else if (s[inputOffset] >= 'A' && s[inputOffset] <= 'F')
            {
                nibble = 10 + s[inputOffset] - 'A';
            }
            else
            {
                return false;
            }

            if (inputOffset % 3 == 0)
            {
                b[inputOffset / 3] = (nibble << 4);
            }
            else
            {
                b[inputOffset / 3] |= (nibble << 0);
            }
        }

        inputOffset++;
    }

    if (s[inputOffset] != '\0')
    {
        return false;
    }

    return true;
}

static void PackJson
(
    Loc_t loc,
    Scan_t *scanp,
    char *jsonp,
    int jsonl
)
{
    uint64_t ts = 0;
    le_result_t res;
    int len;
    uint16_t hours;
    uint16_t minutes;
    uint16_t seconds;
    uint16_t millis;
    uint16_t years;
    uint16_t months;
    uint16_t days;
    time_t rawtime;
    struct tm * timeinfo;

    res = le_pos_GetTime(&hours, &minutes, &seconds, &millis);
    if (res == LE_OK){
        res = le_pos_GetDate(&years, &months, &days);
        if (res == LE_OK){
            time ( &rawtime );
            timeinfo = localtime ( &rawtime );
            timeinfo->tm_year = years - 1900;
            timeinfo->tm_mon = months - 1;
            timeinfo->tm_mday = days;
            timeinfo->tm_hour = hours;
            timeinfo->tm_min = minutes;
            timeinfo->tm_sec = seconds;

            time_t converted = mktime(timeinfo);
            ts = (uint64_t)converted * 1000;
        } else {
            LE_ERROR("Could not get position date: %s", LE_RESULT_TXT(res));
            ts = GetCurrentTimestamp();
        }
    } else {
        LE_ERROR("Could not get position time: %s", LE_RESULT_TXT(res));
        ts = GetCurrentTimestamp();
    }

    if (loc == GPS)
        len = snprintf(jsonp, jsonl,
                       "{ \"lat\": %lf, \"lon\": %lf, \"hAcc\": %lf,"
                       " \"alt\": %lf, \"vAcc\": %lf, \"fixType\" : \"GNSS\", \"ts\" : %ju}",
                       scanp->lat, scanp->lon, scanp->hAccuracy,
                       scanp->alt, scanp->vAccuracy, (uintmax_t)ts);
    else if (loc == WIFI)
        len = snprintf(jsonp, jsonl,
                       "{ \"lat\": %lf, \"lon\": %lf, \"hAcc\": %lf,"
                       " \"fixType\" : \"WIFI\", \"ts\" : %ju}",
                       scanp->lat, scanp->lon, scanp->hAccuracy, (uintmax_t)ts);
    else
        LE_FATAL("ILLEGAL Location Type: WIFI|GPS");

    if (len >= jsonl)
    {
        LE_FATAL("JSON string (len %d) is longer than buffer (size %zu).", len, jsonl);
    }
}

static void LocationResultHandler(
    ma_combainLocation_LocReqHandleRef_t handle, ma_combainLocation_Result_t result, void *context)
{
    switch (result)
    {
    case MA_COMBAINLOCATION_RESULT_SUCCESS:
    {
        Scan_t scan;
        char json[256];


        const le_result_t res = ma_combainLocation_GetSuccessResponse(
            handle, &scan.lat, &scan.lon, &scan.hAccuracy);
        if (res != LE_OK)
        {
            LE_INFO(
                "Received result notification of type success response, but couldn't fetch the result\n");
            exit(1);
        }
        else
        {
            LE_INFO("Location: latitude=%f, longitude=%f, accuracy=%f meters\n",
                    scan.lat, scan.lon, scan.hAccuracy);
            PackJson(WIFI, &scan, json, sizeof(json));
            LE_INFO("Sent dhub json: %s", json);
            psensor_PushJson(saved_ref, 0 /* now */, json);
            saved_ref = NULL;
        }
        break;
    }

    case MA_COMBAINLOCATION_RESULT_ERROR:
    {
        char firstDomain[65];
        char firstReason[65];
        char firstMessage[129];
        uint16_t code;
        char message[129];

        const le_result_t res = ma_combainLocation_GetErrorResponse(
            handle,
            firstDomain,
            sizeof(firstDomain) - 1,
            firstReason,
            sizeof(firstReason) - 1,
            firstMessage,
            sizeof(firstMessage) - 1,
            &code,
            message,
            sizeof(message) - 1);
        if (res != LE_OK)
        {
            LE_INFO(
                "Received result notification of type success response, but couldn't fetch the result\n");
            exit(1);
        }
        else
        {
            LE_INFO("Received an error response.\n");
            LE_INFO("  firstDomain: %s\n", firstDomain);
            LE_INFO("  firstReason: %s\n", firstReason);
            LE_INFO("  firstMessage: %s\n", firstMessage);
            LE_INFO("  code: %d\n", code);
            LE_INFO("  message: %s\n", message);
            exit(1);
        }

        break;
    }

    case MA_COMBAINLOCATION_RESULT_RESPONSE_PARSE_FAILURE:
    {
        char rawResult[257];
        const le_result_t res = ma_combainLocation_GetParseFailureResult(
            handle, rawResult, sizeof(rawResult) - 1);
        if (res != LE_OK)
        {
            LE_INFO(
                "Received result notification of type success response, but couldn't fetch the result\n");
            exit(1);
        }
        else
        {
            LE_INFO("Received a result which couldn't be parsed \"%s\"\n", rawResult);
            exit(1);
        }
        break;
    }

    case MA_COMBAINLOCATION_RESULT_COMMUNICATION_FAILURE:
        LE_INFO("Couldn't communicate with Combain server\n");
        exit(1);
        break;

    default:
        LE_INFO("Received unhandled result type (%d)\n", result);
        exit(1);
    }
}

static bool TrySubmitRequest(void)
{
    if (!State.waitingForWifiResults)
    {
        LE_INFO("Attempting to submit location request");
        const le_result_t res = ma_combainLocation_SubmitLocationRequest(
            State.combainHandle, LocationResultHandler, NULL);
        if (res != LE_OK)
            LE_FATAL("Failed to submit location request\n");
        LE_INFO("Submitted request handle: %d", (uint32_t) State.combainHandle);

        return true;
    }

    LE_INFO("Cannot submit WIFI location request to Combain as previous in transit");
    return false;
}

static void WifiEventHandler(le_wifiClient_Event_t event, void *context)
{
    LE_INFO("Called WifiEventHandler() with event=%d", event);
    switch (event)
    {
    case LE_WIFICLIENT_EVENT_SCAN_DONE:
        if (State.waitingForWifiResults)
        {
            State.waitingForWifiResults = false;

            le_wifiClient_AccessPointRef_t ap = le_wifiClient_GetFirstAccessPoint();
            while (ap != NULL)
            {
                uint8_t ssid[32];
                size_t ssidLen = sizeof(ssid);
                char bssid[(2 * 6) + (6 - 1) + 1]; // "nn:nn:nn:nn:nn:nn\0"
                int16_t signalStrength;
                le_result_t res;
                uint8_t bssidBytes[6];
                res = le_wifiClient_GetSsid(ap, ssid, &ssidLen);
                if (res != LE_OK)
                {
                    LE_INFO("Failed while fetching WiFi SSID\n");
                    exit(1);
                }

                res = le_wifiClient_GetBssid(ap, bssid, sizeof(bssid) - 1);
                if (res != LE_OK)
                {
                    LE_INFO("Failed while fetching WiFi BSSID\n");
                    exit(1);
                }

                // TODO: LE-10254 notes that an incorrect error code of 0xFFFF is mentioned in the
                // documentation. The error code used in the implementation is 0xFFF.
                signalStrength = le_wifiClient_GetSignalStrength(ap);
                if (signalStrength == 0xFFF)
                {
                    LE_INFO("Failed while fetching WiFi signal strength\n");
                    exit(1);
                }

                if (!MacAddrStringToBinary(bssid, bssidBytes))
                {
                    LE_INFO("WiFi scan contained invalid bssid=\"%s\"\n", bssid);
                    exit(1);
                }

                res = ma_combainLocation_AppendWifiAccessPoint(
                    State.combainHandle, bssidBytes, 6, ssid, ssidLen, signalStrength);
                LE_INFO("Submitted AccessPoint: %d ssid: %s", (uint32_t) State.combainHandle,
                        (char *) ssid);

                if (res != LE_OK)
                {
                    LE_INFO("Failed to append WiFi scan results to combain request\n");
                    exit(1);
                }

                ap = le_wifiClient_GetNextAccessPoint();
            }

            TrySubmitRequest();
        }
        break;

    case LE_WIFICLIENT_EVENT_SCAN_FAILED:
        LE_INFO("WiFi scan failed\n");
        exit(1);
        break;

    default:
        // Do nothing - don't care about connect/disconnect events
        break;
    }
}

static void Sample
(
    psensor_Ref_t ref
)
{
    Scan_t scan;
    char json[256];

    int32_t lat;
    int32_t lon;
    int32_t hAccuracy;
    int32_t alt;
    int32_t vAccuracy;

    le_result_t posRes = le_pos_Get3DLocation(&lat, &lon, &hAccuracy, &alt, &vAccuracy);

    /* LE_INFO("le_pos_Get3DLocation returned: lat = %d lon: %d hAccuracy: %d alt: %d vAccuracy: %d",
       lat, lon, hAccuracy, alt, vAccuracy); */

    scan.lat = (double) lat / 1000000.0 ;
    scan.lon = (double) lon / 1000000.0;
    scan.hAccuracy = (double) hAccuracy;
    scan.alt = (double) alt / 1000.0;
    scan.vAccuracy = (double) vAccuracy;

    /* LE_INFO("CVT double: lat = %f lon: %f hAccuracy: %f alt: %f vAccuracy: %f",
       scan.lat, scan.lon, scan.hAccuracy, scan.alt, scan.vAccuracy); */

    // No GPS or low accuracy GPS try Wifi Scan & Combain translation
    if (posRes != LE_OK || scan.hAccuracy > HACCURACY_GOOD) {
        le_result_t startRes;

        if (!State.waitingForWifiResults) {
            /* Legato WIFI is broken so we need to create a fake access point to do a scan */
            const char *ssidPtr = "mangOH";
            le_wifiClient_AccessPointRef_t  createdAccessPoint;

            createdAccessPoint = le_wifiClient_Create((const uint8_t *)ssidPtr, strlen(ssidPtr));
            if (NULL == createdAccessPoint)
            {
                LE_INFO("le_wifiClient_Create returns NULL.\n");
                exit(1);
            }

            startRes = le_wifiClient_SetInterface(createdAccessPoint, "wlan1");
            if (LE_OK != startRes)
            {
                LE_FATAL("ERROR: le_wifiClient_SetInterface returns %d.\n", startRes);
                exit(1);
            }

            startRes = le_wifiClient_Start();

            if (startRes != LE_OK && startRes != LE_BUSY) {
                LE_FATAL("Couldn't start the WiFi service error code: %s", LE_RESULT_TXT(startRes));
                exit(1);
            }

            le_wifiClient_Scan();
            State.combainHandle = ma_combainLocation_CreateLocationRequest();
            LE_INFO("Create request handle: %d", (uint32_t) State.combainHandle);
            saved_ref = ref;
            State.waitingForWifiResults = true;
        }
        else
            LE_INFO("le_wifiClient_Scan still RUNNING");
    }

    // Good GPS
    else if (posRes == LE_OK && scan.hAccuracy <= HACCURACY_GOOD)
    {
        PackJson(GPS, &scan, json, sizeof(json));
        psensor_PushJson(ref, 0 /* now */, json);

        // Kill any errant WIFI sacn requests as we got GPS
        if (State.waitingForWifiResults)
            State.waitingForWifiResults = false;
    }

    else
        LE_ERROR("SHOULD NOT REACH HERE");

}


COMPONENT_INIT
{
    // Activate the positioning service.
    le_posCtrl_ActivationRef_t posCtrlRef = le_posCtrl_Request();
    LE_FATAL_IF(posCtrlRef == NULL, "Couldn't activate positioning service");

    State.wifiHandler = le_wifiClient_AddNewEventHandler(WifiEventHandler, NULL);

    // Use the periodic sensor component from the Data Hub to implement the timer and Data Hub
    // interface.  We'll provide samples as JSON structures.
    psensor_Create("coordinates", DHUBIO_DATA_TYPE_JSON, "", Sample);

    dhubIO_SetJsonExample("coordinates/value", "{\"lat\":0.1,\"lon\":0.2,"
                          "\"alt\":0.3,\"hAcc\":0.4,\"vAcc\":0.5,\"fixType\":\"GNSS\"}");
}

