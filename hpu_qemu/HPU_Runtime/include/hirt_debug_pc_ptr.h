/*
 * @Descripttion: 
 * @version: 
 * @Author: AlonzoChen
 * @Date: 2021-01-12 17:39:29
 * @LastEditors: AlonzoChen
 * @LastEditTime: 2021-05-04 14:12:44
 */
#ifndef __LIB_HIRT_DEBUG_PC_PTR__
#define __LIB_HIRT_DEBUG_PC_PTR__

#include <pthread.h>
#include <semaphore.h>
#include "hirt.h"
#include "hirt_scheduler.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct
{
    pthread_t m_thread;
    hirtThreadFunction_t m_threadfunc;
    void* m_pArg;

    // hirtScoreboard_t *m_pScoreboard;
    // sem_t *m_pSemScheduler;
} hirtDebugPC_PTRHandler_t;

hisdkRet_t hirtDebugPC_PTRHandlerCreate(hirtDebugPC_PTRHandler_t **ppHandler);
hisdkRet_t hirtDebugPC_PTRHandlerDestroy(hirtDebugPC_PTRHandler_t *pHandler);
void*      hirtDebugPC_PTRHandlerThread(void* arg);

#ifdef __cplusplus
}
#endif
#endif /*__LIB_HIRT_DEBUG_PC_PTR__*/

