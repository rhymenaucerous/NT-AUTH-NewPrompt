/**

    @file      Threads.c
    @brief     Threadpool management functions.

**/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Custom includes
#include "WinDebugCustom.h"
#include "WinMem.h"

// Project includes
#include "Shared.h"

DEMO_STATUS
ThreadPoolInit(PTHREADPOOL_CTX *ppThreadPoolCtx)
{
    DEMO_STATUS     Return         = DEMO_ERR_GENERIC;
    BOOL            bRet           = FALSE;
    PTHREADPOOL_CTX pThreadPoolCtx = NULL;

    if (NULL == ppThreadPoolCtx)
    {
        DEBUG_ERROR("Invalid parameter");
        Return = DEMO_INVALID_PARAM;
        goto EXIT;
    }

    pThreadPoolCtx
        = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(THREADPOOL_CTX));
    if (NULL == pThreadPoolCtx)
    {
        DEBUG_PRINT("Failed to create threadpool structure");
        goto EXIT;
    }

    *ppThreadPoolCtx = pThreadPoolCtx;

    InitializeThreadpoolEnvironment(&pThreadPoolCtx->CallBackEnviron);

    pThreadPoolCtx->pThreadPool = CreateThreadpool(NULL);
    if (NULL == pThreadPoolCtx->pThreadPool)
    {
        DEBUG_PRINT("Failed to create threadpool");
        goto EXIT;
    }

    bRet = SetThreadpoolThreadMinimum(pThreadPoolCtx->pThreadPool, 1);
    if (FALSE == bRet)
    {
        DEBUG_ERROR("Failed to set minimum threads for threadpool");
        goto EXIT;
    }

    pThreadPoolCtx->pCleanupGroup = CreateThreadpoolCleanupGroup();
    if (NULL == pThreadPoolCtx->pCleanupGroup)
    {
        DEBUG_ERROR("Failed to create cleanup group for threadpool");
        goto EXIT;
    }

    SetThreadpoolCallbackPool(&pThreadPoolCtx->CallBackEnviron,
                              pThreadPoolCtx->pThreadPool);

    SetThreadpoolCallbackCleanupGroup(
        &pThreadPoolCtx->CallBackEnviron, pThreadPoolCtx->pCleanupGroup, NULL);

    Return = DEMO_SUCCESS;
EXIT:
    if (DEMO_SUCCESS != Return)
    {
        ThreadPoolDestroy(ppThreadPoolCtx);
    }
    return Return;
} // ThreadPoolInit

DEMO_STATUS
ThreadPoolWork(PTHREADPOOL_CTX   pThreadPoolCtx,
               PTP_WORK_CALLBACK CallbackFunc,
               PVOID             Parameter)
{
    DEMO_STATUS Return = DEMO_ERR_GENERIC;
    PTP_WORK    pWork  = NULL;

    if (NULL == pThreadPoolCtx || NULL == CallbackFunc || NULL == Parameter)
    {
        DEBUG_ERROR("Invalid parameter");
        Return = DEMO_INVALID_PARAM;
        goto EXIT;
    }

    pWork = CreateThreadpoolWork(
        CallbackFunc, Parameter, &pThreadPoolCtx->CallBackEnviron);
    if (NULL == pWork)
    {
        DEBUG_PRINT("Failed to create threadpool work item");
        goto EXIT;
    }

    SubmitThreadpoolWork(pWork);

    Return = DEMO_SUCCESS;
EXIT:
    return Return;
} // ThreadPoolWork

VOID
ThreadPoolDestroy (PTHREADPOOL_CTX *ppThreadPoolCtx)
{
    PTHREADPOOL_CTX pThreadPoolCtxLocal = NULL;

    if ((NULL == ppThreadPoolCtx) || (NULL == *ppThreadPoolCtx))
    {
        DEBUG_PRINT("Input NULL");
        goto EXIT;
    }

    pThreadPoolCtxLocal = *ppThreadPoolCtx;

    if (NULL != pThreadPoolCtxLocal->pThreadPool)
    {
        // This function will wait for all callbacks to complete.
        CloseThreadpoolCleanupGroupMembers(
            pThreadPoolCtxLocal->pCleanupGroup, FALSE, NULL);
        CloseThreadpoolCleanupGroup(pThreadPoolCtxLocal->pCleanupGroup);
        CloseThreadpool(pThreadPoolCtxLocal->pThreadPool);
    }

    ZeroingHeapFree((PVOID *)ppThreadPoolCtx, sizeof(THREADPOOL_CTX));

EXIT:
    return;
} // ThreadPoolDestroy

// End of file
