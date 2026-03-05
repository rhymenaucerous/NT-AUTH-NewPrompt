/**

    @file      Priv.c
    @brief     Source file for privilege setting functions.

**/

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "Shared.h"

// Custom Includes
#include "WinDebugCustom.h"
#include "WinMem.h"

DEMO_STATUS
SetDebugPrivilege()
{
    DEMO_STATUS Status  = DEMO_ERR_GENERIC;
    BOOL        bStatus = FALSE;
    HANDLE      hToken  = NULL;

    bStatus = OpenProcessToken(
        GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken);
    if (NULL == hToken || FALSE == bStatus)
    {
        DEBUG_ERROR("OpenProcessToken");
        goto EXIT;
    }

    Status = SetPrivilege(hToken, SE_DEBUG_NAME, TRUE); // To enable openprocess
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("SetPrivilege failed");
        goto EXIT;
    }

    Status = DEMO_SUCCESS;
EXIT:
    if (hToken)
    {
        CloseHandle(hToken);
    }
    return Status;
} // SetDebugPrivilege

DEMO_STATUS
SetPrivilege(HANDLE hToken, PWCHAR pPrivilegeName, BOOL bEnablePrivilege)
{
    DEMO_STATUS      Status  = DEMO_ERR_GENERIC;
    TOKEN_PRIVILEGES tp      = { 0 };
    LUID             luid    = { 0 };
    BOOL             bStatus = FALSE;

    // Set privileges on the local system
    bStatus = LookupPrivilegeValueW(NULL, pPrivilegeName, &luid);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("LookupPrivilegeValueW failed");
        goto EXIT;
    }

    tp.PrivilegeCount     = 1;
    tp.Privileges[0].Luid = luid;
    if (bEnablePrivilege)
    {
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    }
    else
    {
        tp.Privileges[0].Attributes = 0;
    }

    bStatus = AdjustTokenPrivileges(
        hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), NULL, NULL);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("AdjustTokenPrivileges failed");
        goto EXIT;
    }

    Status = DEMO_SUCCESS;
EXIT:
    return Status;
} // SetPrivilege

// End of file
