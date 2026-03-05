/**

    @file      CmdPrompt.c
    @brief

**/

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Custom Includes
#include "WinDebugCustom.h"
#include "WinMem.h"
#include "WinSignal.h"

// Project includes
#include "Shared.h"

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

/**
    @brief  Simple function to simulate pressing the Enter key by writing a
KEY_EVENT record to the console input buffer. This is used to wake up the user
input thread if it's waiting on a blocking call, allowing it to check the
            shutdown event and exit in a timely manner when the process is
            shutting down.
**/
static VOID PressEnter(HANDLE hInputHandle);

// ############################## Fn Definitions ##############################

DEMO_STATUS
CreateCmdInstance(HANDLE hImpersonationToken)
{
    DEMO_STATUS         Status      = DEMO_ERR_GENERIC;
    BOOL                bStatus     = FALSE;
    DEMO_CONFIG         DemoConfig  = { 0 };
    PDEMO_CONFIG        pDemoConfig = &DemoConfig;
    STARTUPINFO         si          = { 0 };
    PROCESS_INFORMATION pi          = { 0 };
    HANDLE              hStdIn      = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE              hHandles[2] = { 0 };

    Status = DemoInit(&DemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Demo initialization failed");
        goto EXIT;
    }

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

    bStatus = CreateProcessWithTokenW(hImpersonationToken,
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

    // Now start two threads for listening for user input and listening for
    // output from the child process, and wait for the child process to exit.
    Status = ThreadPoolWork(
        pDemoConfig->pThreadPoolCtx, HandleUserInput, pDemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create user input thread");
        if (NULL != pDemoConfig->hShutdownEvent)
        {
            SetEvent(pDemoConfig->hShutdownEvent);
        }
    }

    Status = ThreadPoolWork(
        pDemoConfig->pThreadPoolCtx, HandleCmdOutput, pDemoConfig);
    if (DEMO_SUCCESS != Status)
    {
        DEBUG_PRINT("Failed to create cmd output thread");
        if (NULL != pDemoConfig->hShutdownEvent)
        {
            SetEvent(pDemoConfig->hShutdownEvent);
        }
    }

    hHandles[0] = pi.hProcess;
    hHandles[1] = pDemoConfig->hShutdownEvent;
    WaitForMultipleObjects(2, hHandles, FALSE, INFINITE);

    // In case the process exited prior to user shutdown
    if (NULL != pDemoConfig->hShutdownEvent)
    {
        SetEvent(pDemoConfig->hShutdownEvent);
    }

    Status = DEMO_SUCCESS;
EXIT:
    // Send data to the cmd input pipe to wake up the user input thread if it's
    // waiting on a blocking call.
    if (NULL != pDemoConfig->hCmdOutputWrite)
    {
        bStatus = WriteFile(pDemoConfig->hCmdOutputWrite,
                            "Returning to normal console...",
                            30,
                            NULL,
                            NULL);
        if (FALSE == bStatus)
        {
            DEBUG_ERROR("WriteFile to cmd output pipe");
        }
    }

    // Press enter on the main console to wake up the user input thread if it's
    // waiting on user input.
    PressEnter(hStdIn);

    // Write exit to the cmd input pipe to shut it down if it's still running.
    if (NULL != pDemoConfig->hCmdInputWrite)
    {
        bStatus
            = WriteFile(pDemoConfig->hCmdInputWrite, "exit\r\n", 6, NULL, NULL);
        if (FALSE == bStatus)
        {
            DEBUG_ERROR("WriteFile to cmd input pipe");
        }
    }

    if (NULL != pi.hProcess)
    {
        // We don't want to orphan this child process.
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    if (pi.hProcess)
    {
        CloseHandle(pi.hProcess);
    }
    if (pi.hThread)
    {
        CloseHandle(pi.hThread);
    }
    DemoCleanup(&DemoConfig);
    return Status;
} // CreateCmdInstance

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
} // DemoInit

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
        CHAR  pBuffer[BUFFER_SIZE] = { 0 };
        DWORD dwCharsRead          = 0;

        bStatus = ReadConsoleA(hInputHandle,
                               pBuffer,
                               BUFFER_SIZE,
                               &dwCharsRead,
                               NULL); // No control character
        if (FALSE == bStatus)
        {
            DEBUG_ERROR("ReadConsoleA");
            goto EXIT;
        }

        bStatus = WriteFile(pDemoConfig->hCmdInputWrite,
                            pBuffer,
                            dwCharsRead,
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

    BOOL  bStatus      = FALSE;
    DWORD dwWaitResult = WAIT_FAILED;

    do
    {
        // +1 to ensure we have room for a null terminator if needed
        CHAR  pBuffer[BUFFER_SIZE + 1] = { 0 };
        DWORD dwBytesRead              = 0;

        bStatus = ReadFile(pDemoConfig->hCmdOutputRead,
                           pBuffer,
                           BUFFER_SIZE,
                           &dwBytesRead,
                           NULL);
        if (FALSE == bStatus || 0 == dwBytesRead)
        {
            DEBUG_PRINT("No more output to read or error occurred");
            break;
        }

        // Conversion to WORD will not cause truncation because BUFFER_SIZE is
        // defined as 1024, which is well within the limits of a WORD.
        printf("%.*s", (WORD)dwBytesRead, pBuffer);

        dwWaitResult = WaitForSingleObject(pDemoConfig->hShutdownEvent, 0);
    } while (WAIT_TIMEOUT == dwWaitResult);

EXIT:
    return;
} // HandleCmdOutput

static VOID
PressEnter (HANDLE hInputHandle)
{
    BOOL         bStatus        = FALSE;
    INPUT_RECORD ir[2]          = { 0 };
    DWORD        dwCharsWritten = 0;

    // Key Down for Enter
    ir[0].EventType                        = KEY_EVENT;
    ir[0].Event.KeyEvent.bKeyDown          = TRUE;
    ir[0].Event.KeyEvent.wVirtualKeyCode   = VK_RETURN;
    ir[0].Event.KeyEvent.uChar.AsciiChar   = '\r';
    ir[0].Event.KeyEvent.dwControlKeyState = 0;
    ir[0].Event.KeyEvent.wRepeatCount      = 1;

    // Key Up for Enter
    ir[1]                         = ir[0];
    ir[1].Event.KeyEvent.bKeyDown = FALSE;

    bStatus = WriteConsoleInputA(hInputHandle, ir, 2, &dwCharsWritten);
    if (FALSE == bStatus)
    {
        DEBUG_ERROR("WriteConsoleInputA");
    }
} // PressEnter

// End of file
