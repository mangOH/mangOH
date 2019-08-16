/*
 * ====================== WARNING ======================
 *
 * THE CONTENTS OF THIS FILE HAVE BEEN AUTO-GENERATED.
 * DO NOT MODIFY IN ANY WAY.
 *
 * ====================== WARNING ======================
 */


#ifndef LE_GPIO_MESSAGES_H_INCLUDE_GUARD
#define LE_GPIO_MESSAGES_H_INCLUDE_GUARD


#include "le_gpio_common.h"

#define _MAX_MSG_SIZE IFGEN_LE_GPIO_MSG_SIZE

// Define the message type for communicating between client and server
typedef struct __attribute__((packed))
{
    uint32_t id;
    uint8_t buffer[_MAX_MSG_SIZE];
}
_Message_t;

#define _MSGID_le_gpio_SetInput 0
#define _MSGID_le_gpio_SetPushPullOutput 1
#define _MSGID_le_gpio_SetTriStateOutput 2
#define _MSGID_le_gpio_SetOpenDrainOutput 3
#define _MSGID_le_gpio_EnablePullUp 4
#define _MSGID_le_gpio_EnablePullDown 5
#define _MSGID_le_gpio_DisableResistors 6
#define _MSGID_le_gpio_Activate 7
#define _MSGID_le_gpio_Deactivate 8
#define _MSGID_le_gpio_SetHighZ 9
#define _MSGID_le_gpio_Read 10
#define _MSGID_le_gpio_AddChangeEventHandler 11
#define _MSGID_le_gpio_RemoveChangeEventHandler 12
#define _MSGID_le_gpio_SetEdgeSense 13
#define _MSGID_le_gpio_DisableEdgeSense 14
#define _MSGID_le_gpio_IsOutput 15
#define _MSGID_le_gpio_IsInput 16
#define _MSGID_le_gpio_GetEdgeSense 17
#define _MSGID_le_gpio_GetPolarity 18
#define _MSGID_le_gpio_IsActive 19
#define _MSGID_le_gpio_GetPullUpDown 20


// Define type-safe pack/unpack functions for all enums, including included types

static inline bool le_gpio_PackPolarity
(
    uint8_t **bufferPtr,
    le_gpio_Polarity_t value
)
{
    return le_pack_PackUint32(bufferPtr, value);
}

static inline bool le_gpio_UnpackPolarity
(
    uint8_t **bufferPtr,
    le_gpio_Polarity_t* valuePtr
)
{
    bool result;
    uint32_t value = 0;
    result = le_pack_UnpackUint32(bufferPtr, &value);
    if (result)
    {
        *valuePtr = value;
    }
    return result;
}

static inline bool le_gpio_PackEdge
(
    uint8_t **bufferPtr,
    le_gpio_Edge_t value
)
{
    return le_pack_PackUint32(bufferPtr, value);
}

static inline bool le_gpio_UnpackEdge
(
    uint8_t **bufferPtr,
    le_gpio_Edge_t* valuePtr
)
{
    bool result;
    uint32_t value = 0;
    result = le_pack_UnpackUint32(bufferPtr, &value);
    if (result)
    {
        *valuePtr = value;
    }
    return result;
}

static inline bool le_gpio_PackPullUpDown
(
    uint8_t **bufferPtr,
    le_gpio_PullUpDown_t value
)
{
    return le_pack_PackUint32(bufferPtr, value);
}

static inline bool le_gpio_UnpackPullUpDown
(
    uint8_t **bufferPtr,
    le_gpio_PullUpDown_t* valuePtr
)
{
    bool result;
    uint32_t value = 0;
    result = le_pack_UnpackUint32(bufferPtr, &value);
    if (result)
    {
        *valuePtr = value;
    }
    return result;
}

// Define pack/unpack functions for all structures, including included types


#endif // LE_GPIO_MESSAGES_H_INCLUDE_GUARD