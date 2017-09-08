//--------------------------------------------------------------------------------------------------
/**
 * This application demonstrates reading the ADC-based light sensor on the mangOH Red and using the
 * same sensor as the boot source to wake up from low power mode.  The app sends out a tweet using
 * the socialService api.  The twitter client must have been previously configured for the tweet to
 * be transmitted successfully.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */
//--------------------------------------------------------------------------------------------------
#include "legato.h"
#include "interfaces.h"
#include <sstream>
#include <memory>
#include "connectAndRun.h"

#define LIGHT_SENSOR_ADC_NUM 3

#define ADC_LEVEL_WAKEUP 1000
#define ADC_LEVEL_SLEEP  400

#define LIGHT_SENSOR_SAMPLE_INTERVAL_MS 2000

static void SendTweet(const char* tweet);
static void LightSensorSampleTimerHandler(le_timer_Ref_t sampleTimer);
static void PushCallbackHandler(le_avdata_PushStatus_t status, void *context);
static void AvSessionStateHandler(le_avdata_SessionState_t state, void *context);
static void TryStartTimer(void);

static const uint32_t UnicodeTiredFace = 0x1F62B;
static const uint32_t UnicodeSleepingFace = 0x1F634;

static le_avdata_RequestSessionObjRef_t AvSession;
static le_timer_Ref_t LightSensorSampleTimer;
static bool TweetPending = false;
static bool AvDataSessionActive = false;
static uint32_t PushCount = 0;
static uint32_t ShutdownAfterPush = 0;


static const char *LightSensorReadingResource = "lightSensor/reading";


//--------------------------------------------------------------------------------------------------
/**
 * Attempts to send out the given tweet and logs errors if it is unsuccessful.
 */
//--------------------------------------------------------------------------------------------------
static void SendTweet
(
    const char* tweet  ///< [IN] text to send out in the tweet
)
{
    LE_DEBUG("Sending out tweet");
    int attempts = 0;
    do
    {
        const auto r = twitter_Tweet(tweet);
        attempts++;
        if (r == LE_OK)
        {
            break;
        }
        else if (r == LE_OVERFLOW)
        {
            LE_ERROR("Wakeup tweet was too long");
            break;
        }
        else if (r == LE_FORMAT_ERROR)
        {
            LE_ERROR("Wakeup tweet was not utf-8 encoded");
            break;
        }
        else if (r == LE_COMM_ERROR)
        {
            LE_DEBUG("Communication error during wakeup tweet on attempt %d", attempts);
        }
        else if (r == LE_NOT_PERMITTED)
        {
            LE_ERROR("Twitter service is not configured");
            break;
        }
        else
        {
            LE_FATAL("Unhandled tweet result (%d)", r);
        }
    } while (attempts < 3);
    TweetPending = false;
}

//--------------------------------------------------------------------------------------------------
/**
 * Timer expiry handler which reads the ADC that is connected to the light sensor and places the
 * device into a low power state if the light level reading is below ADC_LEVEL_SLEEP.
 */
//--------------------------------------------------------------------------------------------------
static void LightSensorSampleTimerHandler
(
    le_timer_Ref_t sampleTimer  ///< Handle of the expired timer
)
{
    int32_t lightSensorReading;
    auto adcResult = le_adc_ReadValue("EXT_ADC3", &lightSensorReading);
    if (adcResult != LE_OK)
    {
        LE_WARN("Failed to read light sensor.  ADC read failed with result code (%d)", adcResult);
        return;
    }
    else
    {
        LE_DEBUG("Light sensor reports value %d", lightSensorReading);
    }


    if (lightSensorReading < ADC_LEVEL_SLEEP)
    {
        ShutdownAfterPush = PushCount + 1;
        LE_INFO(
            "Initiating shutdown due to light sensor value %d which is below the minimum %d",
            lightSensorReading,
            ADC_LEVEL_SLEEP);
    }

    auto setIntRes = le_avdata_SetInt(LightSensorReadingResource, lightSensorReading);
    if (setIntRes != LE_OK)
    {
        LE_WARN("Failed to set light sensor resource");
    }
    else
    {
        PushCount++;
        le_avdata_Push(LightSensorReadingResource, PushCallbackHandler, (void *)PushCount);
    }
}

static void PushCallbackHandler
(
    le_avdata_PushStatus_t status, ///< push status
    void *context                  ///< The push count when the push was submitted
)
{
    switch (status)
    {
    case LE_AVDATA_PUSH_SUCCESS:
        break;

    case LE_AVDATA_PUSH_FAILED:
        LE_WARN("Failed to push push_button state");
        break;

    default:
        LE_FATAL("Unexpected push status");
        break;
    }

    if ((uint32_t)context == ShutdownAfterPush)
    {
        if (le_ulpm_BootOnAdc(
                LIGHT_SENSOR_ADC_NUM,
                LIGHT_SENSOR_SAMPLE_INTERVAL_MS,
                0.0,
                ADC_LEVEL_WAKEUP) != LE_OK)
        {
            LE_ERROR("Failed to configure ADC3 as a wakeup source");
            return;
        }

        auto shutdownResult = le_ulpm_ShutDown();
        switch (shutdownResult)
        {
        case LE_OK:
            LE_DEBUG("Shutdown initiated successfully");
            break;

        case LE_NOT_POSSIBLE:
            LE_WARN("Can't shutdown the system right now");
            break;

        case LE_FAULT:
            LE_ERROR("Failed to initiate shutdown");
            break;

        default:
            LE_FATAL("Unhandled shutdown result (%d)", shutdownResult);
            break;
        }
    }
}

static void AvSessionStateHandler
(
    le_avdata_SessionState_t state,
    void *context
)
{
    switch (state)
    {
    case LE_AVDATA_SESSION_STARTED:
        LE_DEBUG("AVData session started");
        AvDataSessionActive = true;
        TryStartTimer();
        break;

    case LE_AVDATA_SESSION_STOPPED:
        LE_INFO("AVData session stopped.  Push will fail.");
        AvDataSessionActive = false;
        le_timer_Stop(LightSensorSampleTimer);
        break;

    default:
        LE_FATAL("Unsupported AV session state %d", state);
        break;
    }
}

static void TryStartTimer(void)
{
    if (AvDataSessionActive && !TweetPending)
    {
        LE_ASSERT_OK(le_timer_Start(LightSensorSampleTimer));
    }
}


COMPONENT_INIT
{
    LE_DEBUG("wakeupAndTweetApp started");

    le_result_t r = le_avdata_CreateResource(LightSensorReadingResource, LE_AVDATA_ACCESS_VARIABLE);
    LE_FATAL_IF(r != LE_OK && r != LE_DUPLICATE, "Couldn't create resource for light sensor reading");

    le_avdata_AddSessionStateHandler(AvSessionStateHandler, NULL);
    AvSession = le_avdata_RequestSession();
    LE_FATAL_IF(AvSession == NULL, "Failed to request avdata session");

    LightSensorSampleTimer = le_timer_Create("light sensor");
    LE_ASSERT_OK(le_timer_SetHandler(LightSensorSampleTimer, LightSensorSampleTimerHandler));
    LE_ASSERT_OK(le_timer_SetMsInterval(LightSensorSampleTimer, LIGHT_SENSOR_SAMPLE_INTERVAL_MS));
    LE_ASSERT_OK(le_timer_SetRepeat(LightSensorSampleTimer, 0));


    if (le_bootReason_WasAdc(LIGHT_SENSOR_ADC_NUM))
    {
        LE_DEBUG("Boot reason was ADC %u", LIGHT_SENSOR_ADC_NUM);
        size_t emojiLength = 4;
        char emoji[emojiLength] = {0};
        LE_ASSERT_OK(le_utf8_EncodeUnicodeCodePoint(UnicodeTiredFace, emoji, &emojiLength));

        char dateTime[128];
        LE_ASSERT_OK(le_clk_GetLocalDateTimeString(
                LE_CLK_STRING_FORMAT_DATE_TIME, dateTime, sizeof(dateTime), NULL));

        char imei[32];
        LE_ASSERT_OK(le_info_GetImei(imei, sizeof(imei)));
        std::ostringstream tweetStream;
        tweetStream << emoji << " mangOH with IMEI=" << imei << " has woken up at " << dateTime;
        auto tweet = std::make_shared<std::string>(tweetStream.str());
        TweetPending = true;
        if (wakeupAndTweet_ConnectAndRun([tweet]{
                    SendTweet(tweet->c_str());
                    TryStartTimer();
                }) != LE_OK)
        {
            TweetPending = false;
            LE_ERROR("Couldn't create data connection to send tweet");
            TryStartTimer();
        }
    }
    else
    {
        LE_DEBUG("Boot reason was not ADC %u", LIGHT_SENSOR_ADC_NUM);
        TryStartTimer();
    }
}
