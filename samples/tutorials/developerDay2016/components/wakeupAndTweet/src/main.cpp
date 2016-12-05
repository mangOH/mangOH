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

#define ADC_LEVEL_WAKEUP 900
#define ADC_LEVEL_SLEEP  600

#define LIGHT_SENSOR_SAMPLE_INTERVAL_MS 2000

static const uint32_t UnicodeTiredFace = 0x1F62B;
static const uint32_t UnicodeSleepingFace = 0x1F634;

static le_timer_Ref_t LightSensorSampleTimer;


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
        LE_INFO(
            "Initiating shutdown due to light sensor value %d which is below the minimum %d",
            lightSensorReading,
            ADC_LEVEL_SLEEP);
        if (le_ulpm_BootOnAdcConfigure(
                LIGHT_SENSOR_ADC_NUM,
                LIGHT_SENSOR_SAMPLE_INTERVAL_MS,
                0,
                ADC_LEVEL_WAKEUP) != LE_OK)
        {
            LE_ERROR("Failed to configure ADC3 as a wakeup source");
            return;
        }

        if (le_ulpm_BootOnAdcSetEnabled(LIGHT_SENSOR_ADC_NUM, true) != LE_OK)
        {
            LE_ERROR("Couldn't enable ADC3 as wakeup source");
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


COMPONENT_INIT
{
    LE_DEBUG("wakeupAndTweetApp started");

    LightSensorSampleTimer = le_timer_Create("light sensor");
    LE_ASSERT_OK(le_timer_SetHandler(LightSensorSampleTimer, LightSensorSampleTimerHandler));
    LE_ASSERT_OK(le_timer_SetMsInterval(LightSensorSampleTimer, LIGHT_SENSOR_SAMPLE_INTERVAL_MS));
    LE_ASSERT_OK(le_timer_SetRepeat(LightSensorSampleTimer, 0));

    // Only start the timer if it will not be done after the tweet is sent
    bool startTimer = true;

    if (le_bootReason_WasAdc(LIGHT_SENSOR_ADC_NUM))
    {
        LE_DEBUG("Boot reason was ADC %u", LIGHT_SENSOR_ADC_NUM);
        size_t emojiLength = 4;
        char emoji[emojiLength] = {0};
        LE_ASSERT_OK(le_utf8_EncodeUnicodeCodePoint(UnicodeTiredFace, emoji, &emojiLength));

        std::ostringstream tweetStream;
        tweetStream << emoji << " I'm just waking up at Sierra Wireless Developer Day 2016. #SierraDD2016";
        auto tweet = std::make_shared<std::string>(tweetStream.str());
        if (wakeupAndTweet_ConnectAndRun([tweet]{
                    SendTweet(tweet->c_str());
                    LE_ASSERT_OK(le_timer_Start(LightSensorSampleTimer));
                }) == LE_OK)
        {
            startTimer = false;
        }
        else
        {
            LE_ERROR("Couldn't create data connection to send tweet");
        }
    }
    else
    {
        LE_DEBUG("Boot reason was not ADC %u", LIGHT_SENSOR_ADC_NUM);
        startTimer = true;
    }

    if (startTimer)
    {
        LE_ASSERT_OK(le_timer_Start(LightSensorSampleTimer));
    }
}
