/**

    @file      WinSignal.c
    @brief

**/

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Custom include
#include "WinDebugCustom.h"

// Global HANDLE for this server's state
HANDLE g_hShutdown = NULL;

/**
    @brief  Console cntrl handler function that sets the shutdown event.

    @param  dwCtrlType - The type of event that triggered to cntrl handler.
**/
static BOOL WINAPI
GracefulShutdown (_In_ DWORD dwCtrlType)
{
    switch (dwCtrlType)
    {
        case CTRL_C_EVENT:
            DEBUG_PRINT("CTRL_C_EVENT received, initiating shutdown...");
            break;
        case CTRL_CLOSE_EVENT:
            DEBUG_PRINT("CTRL_CLOSE_EVENT received, initiating shutdown...");
            break;
        case CTRL_BREAK_EVENT:
            DEBUG_PRINT("CTRL_BREAK_EVENT received, initiating shutdown...");
            break;
        case CTRL_LOGOFF_EVENT:
            DEBUG_PRINT("CTRL_LOGOFF_EVENT received, initiating shutdown...");
            break;
        case CTRL_SHUTDOWN_EVENT:
            DEBUG_PRINT("CTRL_SHUTDOWN_EVENT received, initiating shutdown...");
            break;
        default:
            DEBUG_PRINT("Unknown control signal received: %lu", dwCtrlType);
    }

    return SetEvent(g_hShutdown);
} // GracefulShutdown

/**
    @brief  Initializes console control signal handling for graceful shutdown.

    @WARNING: User is responsible for closing the returned event handle.

    @retval  - HANDLE to shutdown event on success, NULL on failure.
**/
HANDLE
WinSignalInitialize()
{
    BOOL bResult = FALSE;

    g_hShutdown = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (NULL == g_hShutdown)
    {
        DEBUG_ERROR("Failed to create shutdown event.");
        goto EXIT;
    }

    bResult = SetConsoleCtrlHandler(GracefulShutdown, TRUE);
    if (FALSE == bResult)
    {
        DEBUG_ERROR("Failed to set console cntrl handler.");
        goto EXIT;
    }

    bResult = TRUE;
EXIT:
    if (FALSE == bResult)
    {
        if (NULL != g_hShutdown)
        {
            CloseHandle(g_hShutdown);
            g_hShutdown = NULL;
        }
    }
    return g_hShutdown;
} // WinSignalInitialize
