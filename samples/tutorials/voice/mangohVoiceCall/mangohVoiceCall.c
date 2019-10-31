/**
 * This application allows you to make phone call on mangOH platform. You will need to have a voice
 * SIM installed on the platform. Also, you need a headset with mic and speaker connected to headset
 * jack CN500.
 *
 * For information on the fundamentals of the voice call API, visit
 * http://legato.io/legato-docs/latest/c_audio.html
 *
 * The voice service connects the modem voice RX to speaker & modem voice TX to Mic.
 *
 * Copyright (C) Sierra Wireless Inc. Use of this work is subject to license.
 */

#include "legato.h"
#include "interfaces.h"


//--------------------------------------------------------------------------------------------------
/**
 * Call Reference.
 */
//--------------------------------------------------------------------------------------------------
static le_voicecall_CallRef_t VoiceCallRef;


//--------------------------------------------------------------------------------------------------
/**
 * handler voice call Reference.
 */
//--------------------------------------------------------------------------------------------------
static le_voicecall_StateHandlerRef_t VoiceCallHandlerRef;


//--------------------------------------------------------------------------------------------------
/**
 * Destination Phone number.
 */
//--------------------------------------------------------------------------------------------------
static char DestinationNumber[LE_MDMDEFS_PHONE_NUM_MAX_BYTES];


//--------------------------------------------------------------------------------------------------
/**
 * Audio safe references. To be understand this more look at
 * http://legato.io/legato-docs/latest/c_audio.html
 */
//--------------------------------------------------------------------------------------------------
static le_audio_StreamRef_t MdmRxAudioRef;
static le_audio_StreamRef_t MdmTxAudioRef;
static le_audio_StreamRef_t HeadsetMicRef;
static le_audio_StreamRef_t HeadsetSpeakerRef;

static le_audio_ConnectorRef_t AudioInputConnectorRef;
static le_audio_ConnectorRef_t AudioOutputConnectorRef;


//--------------------------------------------------------------------------------------------------
/**
 * Close Audio Stream.
 */
//--------------------------------------------------------------------------------------------------
static void DisconnectAllAudio
(
    le_voicecall_CallRef_t reference
)
{
    LE_INFO("DisconnectAllAudio");

    if (AudioInputConnectorRef)
    {
        LE_INFO("Disconnect %p from connector.%p", HeadsetMicRef, AudioInputConnectorRef);
        if (HeadsetMicRef)
        {
            LE_INFO("Disconnect %p from connector.%p", HeadsetMicRef, AudioInputConnectorRef);
            le_audio_Disconnect(AudioInputConnectorRef, HeadsetMicRef);
        }
        if (MdmTxAudioRef)
        {
            LE_INFO("Disconnect %p from connector.%p", MdmTxAudioRef, AudioInputConnectorRef);
            le_audio_Disconnect(AudioInputConnectorRef, MdmTxAudioRef);
        }
    }
    if (AudioOutputConnectorRef)
    {
        LE_INFO(
            "le_audio_Disconnect %p from connector.%p", MdmTxAudioRef, AudioOutputConnectorRef);
        if (HeadsetSpeakerRef)
        {
            LE_INFO("Disconnect %p from connector.%p", HeadsetSpeakerRef, AudioOutputConnectorRef);
            le_audio_Disconnect(AudioOutputConnectorRef, HeadsetSpeakerRef);
        }
        if (MdmRxAudioRef)
        {
            LE_INFO("Disconnect %p from connector.%p", MdmRxAudioRef, AudioOutputConnectorRef);
            le_audio_Disconnect(AudioOutputConnectorRef, MdmRxAudioRef);
        }
    }

    if (AudioInputConnectorRef)
    {
        le_audio_DeleteConnector(AudioInputConnectorRef);
        AudioInputConnectorRef = NULL;
    }
    if (AudioOutputConnectorRef)
    {
        le_audio_DeleteConnector(AudioOutputConnectorRef);
        AudioOutputConnectorRef = NULL;
    }

    if (HeadsetMicRef)
    {
        le_audio_Close(HeadsetMicRef);
        HeadsetMicRef = NULL;
    }
    if (HeadsetSpeakerRef)
    {
        le_audio_Close(HeadsetSpeakerRef);
        HeadsetSpeakerRef = NULL;
    }
    if (MdmRxAudioRef)
    {
        le_audio_Close(MdmRxAudioRef);
        MdmRxAudioRef = NULL;
    }
    if (MdmTxAudioRef)
    {
        le_audio_Close(MdmTxAudioRef);
        MdmTxAudioRef = NULL;
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Open Audio Stream.
 */
//--------------------------------------------------------------------------------------------------
static le_result_t OpenAudio
(
    le_voicecall_CallRef_t reference
)
{
    le_result_t res;

    MdmRxAudioRef = le_voicecall_GetRxAudioStream(reference);
    LE_ERROR_IF((MdmRxAudioRef == NULL), "le_voicecall_GetRxAudioStream returns NULL!");

    MdmTxAudioRef = le_voicecall_GetTxAudioStream(reference);
    LE_ERROR_IF((MdmTxAudioRef == NULL), "le_voicecall_GetRxAudioStream returns NULL!");

    LE_DEBUG("OpenAudio MdmRxAudioRef %p, MdmTxAudioRef %p", MdmRxAudioRef, MdmTxAudioRef);

    LE_INFO("Connect to Mic and Speaker");

    // Redirect audio to the Headset  Microphone and Speaker.
    HeadsetSpeakerRef = le_audio_OpenSpeaker();
    LE_ERROR_IF((HeadsetSpeakerRef == NULL), "le_audio_OpenSpeaker returns NULL!");
    HeadsetMicRef = le_audio_OpenMic();
    LE_ERROR_IF((HeadsetMicRef == NULL), "le_audio_OpenMic returns NULL!");

    AudioInputConnectorRef = le_audio_CreateConnector();
    LE_ERROR_IF((AudioInputConnectorRef == NULL), "AudioInputConnectorRef is NULL!");
    AudioOutputConnectorRef = le_audio_CreateConnector();
    LE_ERROR_IF((AudioOutputConnectorRef == NULL), "AudioOutputConnectorRef is NULL!");

    if (MdmRxAudioRef && MdmTxAudioRef && HeadsetSpeakerRef && HeadsetMicRef &&
        AudioInputConnectorRef && AudioOutputConnectorRef)
    {
        res = le_audio_Connect(AudioInputConnectorRef, HeadsetMicRef);
        LE_ERROR_IF((res != LE_OK), "Failed to connect RX on Input connector!");
        res = le_audio_Connect(AudioInputConnectorRef, MdmTxAudioRef);
        LE_ERROR_IF((res != LE_OK), "Failed to connect mdmTx on Input connector!");
        res = le_audio_Connect(AudioOutputConnectorRef, HeadsetSpeakerRef);
        LE_ERROR_IF((res != LE_OK), "Failed to connect TX on Output connector!");
        res = le_audio_Connect(AudioOutputConnectorRef, MdmRxAudioRef);
        LE_ERROR_IF((res != LE_OK), "Failed to connect mdmRx on Output connector!");
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * Handler function for Call Event Notifications.
 */
//--------------------------------------------------------------------------------------------------
static void MyCallEventHandler
(
    le_voicecall_CallRef_t reference,
    const char* identifier,
    le_voicecall_Event_t callEvent,
    void* contextPtr
)
{
    le_result_t res;
    le_voicecall_TerminationReason_t term = LE_VOICECALL_TERM_UNDEFINED;

    LE_INFO(
        "Voice Call : New Call event: %d for Call %p, from %s", callEvent, reference, identifier);

    if (callEvent == LE_VOICECALL_EVENT_ALERTING)
    {
        LE_INFO("Check MyCallEventHandler passed, event is LE_VOICECALL_EVENT_ALERTING.");
    }
    else if (callEvent == LE_VOICECALL_EVENT_CONNECTED)
    {
        OpenAudio(reference);
        LE_INFO("Check MyCallEventHandler passed, event is LE_VOICECALL_EVENT_CONNECTED.");
    }
    else if (callEvent == LE_VOICECALL_EVENT_TERMINATED)
    {
        DisconnectAllAudio(reference);
        LE_INFO("Check MyCallEventHandler passed, event is LE_VOICECALL_EVENT_TERMINATED.");
        le_voicecall_GetTerminationReason(reference, &term);
        switch (term)
        {
            case LE_VOICECALL_TERM_NETWORK_FAIL:
            {
                LE_ERROR("Termination reason is LE_VOICECALL_TERM_NETWORK_FAIL");
            }
            break;

            case LE_VOICECALL_TERM_BUSY:
            {
                LE_ERROR("Termination reason is LE_VOICECALL_TERM_BUSY");
            }
            break;

            case LE_VOICECALL_TERM_LOCAL_ENDED:
            {
                LE_INFO("LE_VOICECALL_TERM_LOCAL_ENDED");
                le_voicecall_RemoveStateHandler(VoiceCallHandlerRef);
            }
            break;

            case LE_VOICECALL_TERM_REMOTE_ENDED:
            {
                LE_INFO("Termination reason is LE_VOICECALL_TERM_REMOTE_ENDED");
                LE_INFO("---!!!! PLEASE CREATE AN INCOMING CALL !!!!---");
            }
            break;

            case LE_VOICECALL_TERM_UNDEFINED:
            {
                LE_INFO("Termination reason is LE_VOICECALL_TERM_UNDEFINED");
                LE_ERROR("---!!!! PLEASE CREATE AN INCOMING CALL !!!!---");
            }
            break;

            default:
            {
                LE_ERROR("Termination reason is %d", term);
            }
            break;
        }

        le_voicecall_Delete(reference);
        exit(EXIT_SUCCESS);
    }
    else if (callEvent == LE_VOICECALL_EVENT_INCOMING)
    {
        LE_INFO("Check MyCallEventHandler passed, event is LE_VOICECALL_EVENT_INCOMING.");
        res = le_voicecall_Answer(reference);
        if (res == LE_OK)
        {
            VoiceCallRef = reference;
            LE_INFO("Check MyCallEventHandler passed, I answered the call");
        }
        else
        {
            LE_ERROR("Check MyCallEventHandler failed to answer the call.");
        }
    }
    else if (callEvent == LE_VOICECALL_EVENT_CALL_END_FAILED)
    {
        LE_INFO("Event is LE_VOICECALL_EVENT_CALL_END_FAILED.");
    }
    else if (callEvent == LE_VOICECALL_EVENT_CALL_ANSWER_FAILED)
    {
        LE_INFO("Event is LE_VOICECALL_EVENT_CALL_ANSWER_FAILED.");
    }
    else
    {
        LE_ERROR("Check MyCallEventHandler failed, unknowm event %d.", callEvent);
    }
}

//--------------------------------------------------------------------------------------------------
/**
 * Create and start a voice call.
 */
//--------------------------------------------------------------------------------------------------

static le_result_t MangohVoicecall_Start(void)
{
    VoiceCallRef = le_voicecall_Start(DestinationNumber);

    return VoiceCallRef ? LE_OK : LE_FAULT;
}

//--------------------------------------------------------------------------------------------------
/*
 * mangOH must be registered on Network with the SIM in ready state.
 * Make sure voiceCallService is started by entering: app start voiceCallService
 * Check "logread -f "
 * Execute app : app runProc  mangohVoiceCall --exe=mangohVoiceCall --  <Destination phone number>
 * Follow INFO instruction in traces.
 */
//--------------------------------------------------------------------------------------------------
COMPONENT_INIT
{
    LE_INFO("############### Voice call test");

    if (le_arg_NumArgs() == 1)
    {
        // This function gets the telephone number from the User.
        const char* phoneNumber = le_arg_GetArg(0);
        strncpy(DestinationNumber, phoneNumber, LE_MDMDEFS_PHONE_NUM_MAX_BYTES - 1);

        LE_INFO("Phone number %s", DestinationNumber);
        LE_ASSERT(MangohVoicecall_Start() == LE_OK);
    }
    else
    {
        LE_INFO(
            "PRINT USAGE => app runProc mangohVoiceCall --exe = mangohVoiceCall -- <Destination "
            "phone number>");
        exit(EXIT_FAILURE);
    }

    VoiceCallHandlerRef = le_voicecall_AddStateHandler(MyCallEventHandler, NULL);
}
