/**

    @file      Shared.h
    @brief     Shared defs for NT-AUTH-UpgradePrompt project.

**/
#pragma once

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define BASE_10        10
#define WIN_LOGON_PATH L"C:\\Windows\\System32\\winlogon.exe"
#define CMD_PATH       L"C:\\Windows\\System32\\cmd.exe"
#define TWO_HANDLES    2
#define BUFFER_SIZE    1024
#define TIME_STR_LEN   32

typedef enum
{
    DEMO_SUCCESS           = 0,   // Operation successful
    DEMO_INVALID_PARAM     = 1,   // Invalid parameter passed
    DEMO_MEMORY_ALLOCATION = 2,   // Memory allocation failure
    DEMO_BAD_PACKET        = 3,   // Bad packet received from server
    DEMO_ERR_GENERIC       = 100, // Generic error
} DEMO_STATUS;

// ############################ Struct Definitions ############################

typedef struct _THREADPOOL_CTX
{
    PTP_POOL            pThreadPool;
    TP_CALLBACK_ENVIRON CallBackEnviron;
    PTP_CLEANUP_GROUP   pCleanupGroup;
} THREADPOOL_CTX, *PTHREADPOOL_CTX;

typedef struct _DEMO_CONFIG
{
    HANDLE hShutdownEvent;

    // Two handles for each of the input and output pipes - one for the parent
    // process and one for the child process.
    HANDLE hCmdInputRead;
    HANDLE hCmdInputWrite;
    HANDLE hCmdOutputRead;
    HANDLE hCmdOutputWrite;

    // Threadpool structures
    PTHREADPOOL_CTX pThreadPoolCtx;
} DEMO_CONFIG, *PDEMO_CONFIG;

// ############################## Cmd Functions ##############################

/**
    @brief  Creates a new cmd.exe process with the impersonated token,
redirecting its input and output through pipes so that the parent process can
            interact with it.
    @param  hImpersonationToken - Handle to the impersonation token to use when
                                  creating the new process.
    @retval             - DEMO_STATUS indicating success or failure.
**/
DEMO_STATUS
CreateCmdInstance(HANDLE hImpersonationToken);

// ############################ Thread Functions ############################

/**
    @brief Initializes the thread pool context
    @param  ppThreadPoolCtx - Pointer to the thread pool context
    @retval                 - DEMO_STATUS indicating success or failure
**/
DEMO_STATUS ThreadPoolInit(PTHREADPOOL_CTX *ppThreadPoolCtx);

/**
    @brief Submits work to the thread pool
    @param  pThreadPoolCtx - Pointer to the thread pool context
    @param  CallbackFunc   - Pointer to the callback function
    @param  Parameter      - Parameter to pass to the callback function
    @retval                - DEMO_STATUS indicating success or failure
**/
DEMO_STATUS
ThreadPoolWork(PTHREADPOOL_CTX   pThreadPoolCtx,
               PTP_WORK_CALLBACK CallbackFunc,
               PVOID             Parameter);

/**
    @brief  Destroys the thread pool context
    @param  ppThreadPoolCtx - Pointer to the thread pool context
**/
VOID ThreadPoolDestroy(PTHREADPOOL_CTX *ppThreadPoolCtx);

// ############################## Priv Functions ##############################

/**
    @brief  Sets the debug privilege for the current process.
**/
DEMO_STATUS SetDebugPrivilege();

/**
    @brief  Set privilege on the provided token.
    @param  hToken           - Token handle.
    @param  pPrivilegeName   - Privilege name to set.
    @param  bEnablePrivilege - TRUE to enable, FALSE to disable.
    @retval                  - DEMO_STATUS indicating success or failure.
**/
DEMO_STATUS SetPrivilege(HANDLE hToken,
                         PWCHAR pPrivilegeName,
                         BOOL   bEnablePrivilege);

// End of file
