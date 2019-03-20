#ifndef COMBAIN_HTTP_H
#define COMBAIN_HTTP_H

#include "legato.h"
#include "interfaces.h"
#include "ThreadSafeQueue.h"

void CombainHttpInit(
    ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> *requestJson,
    ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> *responseJson,
    le_event_Id_t responseAvailableEvent);
void CombainHttpDeinit(void);
void *CombainHttpThreadFunc(void *context);

#endif // COMBAIN_HTTP_H
