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

// Custom Includes
#include "WinDebugCustom.h"
#include "WinMem.h"
#include "WinSignal.h"

// Project includes
#include "Shared.h"

// ############################ Struct Definitions ############################

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

    // Impersonation tokens information
    DWORD  dwTargetProcId;
    HANDLE hToken;
    HANDLE hImpersonationToken;
    HANDLE TargetProcHandle;
} DEMO_CONFIG, *PDEMO_CONFIG;

// ############################# Fn Declarations #############################

/**
    @brief  Initializes the demo configuration, including setting up
            synchronization primitives and the thread pool.
    @param  pDemoConfig - Pointer to the demo configuration structure to
                          initialize.
    @retval             - DEMO_SUCCESS if initialization is successful,
                          otherwise DEMO_ERR_GENERIC.
**/
static DEMO_STATUS DemoInit(PDEMO_CONFIG pDemoConfig);

/**
    @brief  Cleans up resources allocated in the demo configuration, including
            closing handles and destroying the thread pool.
    @param  pDemoConfig - Pointer to the demo configuration structure.
**/
static VOID DemoCleanup(PDEMO_CONFIG pDemoConfig);

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

/**
    @brief  Creates a new cmd.exe process with the impersonated token,
redirecting its input and output through pipes so that the parent process can
            interact with it.
    @param  pDemoConfig - Pointer to the demo configuration structure.
    @retval             - DEMO_STATUS indicating success or failure.
**/
static DEMO_STATUS CreateCmdInstance(PDEMO_CONFIG pDemoConfig);

/**
    @brief  Creates pipes for redirecting the child process's input and
            output, and sets the appropriate handles in the demo configuration
            structure.
    @param  pDemoConfig - Pointer to the demo configuration structure.
    @retval DEMO_STATUS - Status of the operation.
**/
static DEMO_STATUS CreateCmdPipes(PDEMO_CONFIG pDemoConfig);

/**
    @brief  Thread for handling user input from the console and writing it to
            the child process's input pipe. This allows the user to interact
            with the cmd.exe process that we launched with the impersonated
            token.

    @param  Instance  - Pointer to the callback instance.
    @param  Parameter - Pointer to the demo configuration structure.
    @param  Work      - Pointer to the work object.
**/
static VOID CALLBACK HandleUserInput(PTP_CALLBACK_INSTANCE Instance,
                                     PVOID                 Parameter,
                                     PTP_WORK              Work);

/**
    @brief  Handles output from the child process by waiting for the output
            pipe to be signaled, reading the output, and printing it to the
            console. This allows the user to see the output from the cmd.exe
            process that we launched with the impersonated token.
    @param  Instance  - Pointer to the callback instance.
    @param  Parameter - Pointer to the demo configuration structure.
    @param  Work      - Pointer to the work object.
**/
static VOID CALLBACK HandleCmdOutput(PTP_CALLBACK_INSTANCE Instance,
                                     PVOID                 Parameter,
                                     PTP_WORK              Work);

// ############################## Fn Definitions ##############################

INT
wmain (INT iArgc, PWCHAR *ppArgv)
{
    UNREFERENCED_PARAMETER(iArgc);
    UNREFERENCED_PARAMETER(ppArgv);

    DEMO_STATUS Status     = DEMO_ERR_GENERIC;
    BOOL        bStatus    = FALSE;
    DEMO_CONFIG DemoConfig = { 0 };

    Status = DemoInit(&DemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Demo initialization failed");
        goto EXIT;
    }

    // Our target process will be winlogon.exe, which runs as SYSTEM and is
    // always running on the machine.
    // It's also running in session 1, so it isn't protected by session
    // isolation.
    // It's not usually a ppl process, so it won't be protected by PPL
    // protections.
    Status = GetProcessIDFromName(WIN_LOGON_PATH, &DemoConfig.dwTargetProcId);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to find target process ID");
        goto EXIT;
    }

    DemoConfig.TargetProcHandle = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DemoConfig.dwTargetProcId);
    if (NULL == DemoConfig.TargetProcHandle)
    {
        DEBUG_ERROR("OpenProcess");
        goto EXIT;
    }

    bStatus = OpenProcessToken(DemoConfig.TargetProcHandle,
                               TOKEN_QUERY | TOKEN_DUPLICATE,
                               &DemoConfig.hToken);
    if (NULL == DemoConfig.hToken || FALSE == bStatus)
    {
        DEBUG_ERROR("OpenProcessToken");
        goto EXIT;
    }

    bStatus = DuplicateTokenEx(DemoConfig.hToken,
                               TOKEN_ALL_ACCESS,
                               NULL,
                               SecurityImpersonation,
                               TokenImpersonation,
                               &DemoConfig.hImpersonationToken);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("DuplicateTokenEx");
        goto EXIT;
    }

    Status = CreateCmdInstance(&DemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create CMD instance");
        goto EXIT;
    }

    Status = DEMO_SUCCESS;
EXIT:
    DemoCleanup(&DemoConfig);
    return Status;
} // wmain

static DEMO_STATUS
DemoInit (PDEMO_CONFIG pDemoConfig)
{
    DEMO_STATUS Status = DEMO_ERR_GENERIC;

    pDemoConfig->hShutdownEvent = WinSignalInitialize();
    if (NULL == pDemoConfig->hShutdownEvent)
    {
        DEBUG_ERROR("WinSignalInitialize");
        goto EXIT;
    }

    Status = ThreadPoolInit(&pDemoConfig->pThreadPoolCtx);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("ThreadPoolInit failed");
        goto EXIT;
    }

    Status = SetDebugPrivilege();
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to set debug privilege");
        goto EXIT;
    }

    Status = DEMO_SUCCESS;
EXIT:
    return Status;
}

static VOID
DemoCleanup (PDEMO_CONFIG pDemoConfig)
{
    if (NULL == pDemoConfig)
    {
        DEBUG_PRINT("Invalid structure passed to DemoCleanup");
        goto EXIT;
    }

    // Shutdown Threadpool -> this will wait for any active threads to finish
    // before continuing with cleanup.
    if (NULL != pDemoConfig->pThreadPoolCtx)
    {
        ThreadPoolDestroy(&pDemoConfig->pThreadPoolCtx);
    }
    // Cleanup token handles
    if (NULL != pDemoConfig->hToken)
    {
        CloseHandle(pDemoConfig->hToken);
    }
    if (NULL != pDemoConfig->hImpersonationToken)
    {
        CloseHandle(pDemoConfig->hImpersonationToken);
    }

    // Cleanup pipe handles: for anonymous pipes, the documentation states that
    // we need to close all handles.
    // https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations
    if (NULL != pDemoConfig->hCmdInputRead)
    {
        CloseHandle(pDemoConfig->hCmdInputRead);
    }
    if (NULL != pDemoConfig->hCmdInputWrite)
    {
        CloseHandle(pDemoConfig->hCmdInputWrite);
    }
    if (NULL != pDemoConfig->hCmdOutputRead)
    {
        CloseHandle(pDemoConfig->hCmdOutputRead);
    }
    if (NULL != pDemoConfig->hCmdOutputWrite)
    {
        CloseHandle(pDemoConfig->hCmdOutputWrite);
    }

    // Cleanup target process handle
    if (NULL != pDemoConfig->hShutdownEvent)
    {
        CloseHandle(pDemoConfig->hShutdownEvent);
    }
EXIT:
    return;
} // DemoCleanup

static DEMO_STATUS
GetProcessIDFromName (PWCHAR pProcessName, PDWORD pdwProcessID)
{
    DEMO_STATUS Status  = DEMO_ERR_GENERIC;
    BOOL        bStatus = FALSE;
    // Buff size of 1024 is used in MSDN example code:
    // https://learn.microsoft.com/en-us/windows/win32/psapi/enumerating-all-processes
    DWORD dwProcesses[BUFFER_SIZE] = { 0 };
    DWORD dwSize                   = 0;
    DWORD dwProcessCount           = 0;

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

static DEMO_STATUS
CreateCmdInstance (PDEMO_CONFIG pDemoConfig)
{
    DEMO_STATUS         Status                    = DEMO_ERR_GENERIC;
    BOOL                bStatus                   = FALSE;
    STARTUPINFO         si                        = { 0 };
    PROCESS_INFORMATION pi                        = { 0 };
    HANDLE              hWaitHandles[TWO_HANDLES] = { 0 };
    hWaitHandles[0]                               = pDemoConfig->hShutdownEvent;

    Status = CreateCmdPipes(pDemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create pipes for cmd process");
        goto EXIT;
    }

    // Get Handles for input and output pipes and give them to the new process
    // via STARTUPINFO.
    si.dwFlags    = STARTF_USESTDHANDLES;
    si.hStdInput  = pDemoConfig->hCmdInputRead;
    si.hStdOutput = pDemoConfig->hCmdOutputWrite;
    si.hStdError  = pDemoConfig->hCmdOutputWrite;

    bStatus = CreateProcessWithTokenW(pDemoConfig->hImpersonationToken,
                                      0,        // No special logon flags
                                      CMD_PATH, // Application name
                                      CMD_PATH, // Command line
                                      CREATE_NO_WINDOW, // Creation flags
                                      NULL, // Environment - use current
                                      NULL, // Current directory - use current
                                      &si,  // Startup info
                                      &pi); // Process information
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("CreateProcessWithTokenW");
        goto EXIT;
    }

    DEBUG_PRINT("Launched process with impersonated token!");

    // Now start two threads for listening for user input and listening for
    // output from the child process, and wait for the child process to exit.
    Status = ThreadPoolWork(
        pDemoConfig->pThreadPoolCtx, HandleUserInput, pDemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create user input thread");
        goto EXIT;
    }

    Status = ThreadPoolWork(
        pDemoConfig->pThreadPoolCtx, HandleCmdOutput, pDemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create cmd output thread");
        goto EXIT;
    }

    hWaitHandles[1] = pi.hProcess;
    WaitForMultipleObjects(TWO_HANDLES, hWaitHandles, FALSE, INFINITE);

    // In case the process exited before the user closes it, signal the threads
    // to exit if they are still running.
    SetEvent(pDemoConfig->hShutdownEvent);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    Status = DEMO_SUCCESS;
EXIT:
    return Status;
} // CreateCmdInstance

static DEMO_STATUS
CreateCmdPipes (PDEMO_CONFIG pDemoConfig)
{
    DEMO_STATUS Status  = DEMO_ERR_GENERIC;
    BOOL        bStatus = FALSE;

    // Defining security attributes for our pipe handles so that they will be
    // inheritable by the new process.
    SECURITY_ATTRIBUTES sa  = { 0 };
    sa.bInheritHandle       = TRUE;
    sa.lpSecurityDescriptor = NULL;
    sa.nLength              = sizeof(SECURITY_ATTRIBUTES);

    bStatus = CreatePipe(&pDemoConfig->hCmdInputRead,
                         &pDemoConfig->hCmdInputWrite,
                         &sa,
                         0); // Not specifying buffer size to use default
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("CreatePipe Input");
        goto EXIT;
    }

    bStatus = CreatePipe(&pDemoConfig->hCmdOutputRead,
                         &pDemoConfig->hCmdOutputWrite,
                         &sa,
                         0); // Not specifying buffer size to use default
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("CreatePipe Output");
        goto EXIT;
    }

    Status = DEMO_SUCCESS;
EXIT:
    return Status;
} // CreateCmdPipes

static VOID CALLBACK
HandleUserInput (PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
    // Instance, Parameter, and Work not used in this example.
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Work);

    PDEMO_CONFIG pDemoConfig = (PDEMO_CONFIG)Parameter;

    if (NULL == pDemoConfig)
    {
        DEBUG_PRINT("Invalid structure passed to HandleUserInput");
        goto EXIT;
    }

    BOOL   bStatus      = FALSE;
    DWORD  dwWaitResult = WAIT_FAILED;
    HANDLE hInputHandle = GetStdHandle(STD_INPUT_HANDLE);

    do
    {
        WCHAR pBuffer[BUFFER_SIZE] = { 0 };
        DWORD dwCharsRead          = 0;

        bStatus = ReadConsoleW(hInputHandle,
                               pBuffer,
                               BUFFER_SIZE,
                               &dwCharsRead,
                               NULL); // No control character
        if (FALSE == bStatus)
        {
            DEBUG_ERROR("ReadConsoleW");
            goto EXIT;
        }

        bStatus = WriteFile(
            pDemoConfig->hCmdInputWrite,
            pBuffer,
            dwCharsRead * sizeof(WCHAR), // Need to specify size in bytes
            NULL,  // Not interested in number of bytes written
            NULL); // No overlapped structure
        if (FALSE == bStatus)
        {
            DEBUG_ERROR("WriteFile");
            goto EXIT;
        }

        // Keep processing user input until we receive a shutdown signal, at
        // which point we will exit the thread and allow the main thread to
        // clean up resources and exit the process.
        dwWaitResult = WaitForSingleObject(pDemoConfig->hShutdownEvent, 0);
    } while (WAIT_TIMEOUT == dwWaitResult);

EXIT:
    return;
} // HandleUserInput

static VOID CALLBACK
HandleCmdOutput (PTP_CALLBACK_INSTANCE Instance, PVOID Parameter, PTP_WORK Work)
{
    // Instance, Parameter, and Work not used in this example.
    UNREFERENCED_PARAMETER(Instance);
    UNREFERENCED_PARAMETER(Work);

    PDEMO_CONFIG pDemoConfig = (PDEMO_CONFIG)Parameter;

    if (NULL == pDemoConfig)
    {
        DEBUG_PRINT("Invalid structure passed to HandleCmdOutput");
        goto EXIT;
    }

    BOOL   bStatus      = FALSE;
    DWORD  dwWaitResult = WAIT_FAILED;
    HANDLE hWaitHandles[TWO_HANDLES]
        = { pDemoConfig->hCmdOutputRead, pDemoConfig->hShutdownEvent };

    dwWaitResult
        = WaitForMultipleObjects(TWO_HANDLES,
                                 hWaitHandles,
                                 FALSE, // Wait for any handle to be signaled
                                 INFINITE); // Wait indefinitely
    while (WAIT_OBJECT_0 == dwWaitResult)   // Output handle is signaled
    {
        // +1 to ensure we have room for a null terminator if needed
        WCHAR pBuffer[BUFFER_SIZE + 1] = { 0 };
        DWORD dwBytesRead              = 0;

        bStatus = ReadFile(pDemoConfig->hCmdOutputRead,
                           pBuffer,
                           BUFFER_SIZE,
                           &dwBytesRead,
                           NULL);
        if (FALSE == bStatus || 0 == dwBytesRead)
        {
            break;
        }
        // Conversion to WORD will not cause truncation because BUFFER_SIZE is
        // defined as 1024, which is well within the limits of a WORD.
        wprintf(L"%.*s", (WORD)(dwBytesRead / sizeof(WCHAR)), pBuffer);

        dwWaitResult = WaitForMultipleObjects(
            TWO_HANDLES, hWaitHandles, FALSE, INFINITE);
    }

EXIT:
    return;
} // HandleCmdOutput

// End of file
