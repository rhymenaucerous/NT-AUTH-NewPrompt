/**

    @file      WinMem.c
    @brief     Source file for custom memory management utilities on Windows.

**/

// Standard C includes
#include <WS2tcpip.h>
#include <WinSock2.h>
#include <Windows.h>

// Self header file inclusion
#include "WinMem.h"

VOID
ZeroingHeapFree (_In_ PVOID *ppMem, _In_ DWORD dwNumBytes)
{
    PVOID pMem = NULL;

    if (NULL == ppMem)
    {
        goto EXIT;
    }

    pMem = *ppMem;

    if (NULL == pMem || 0 == dwNumBytes)
    {
        goto EXIT;
    }

    SecureZeroMemory(pMem, dwNumBytes);
    HeapFree(GetProcessHeap(), 0, pMem);
    *ppMem = NULL;

EXIT:
    return;
} // ZeroingHeapFree

VOID
PrintBytesInHex (PVOID pData, SIZE_T szNum)
{
    SIZE_T szIndex   = 0;
    PCHAR  pDataChar = pData;

    if (NULL == pData || 0 == szNum)
    {
        return;
    }

    for (szIndex = 0; szIndex < szNum; szIndex++)
    {
        printf("%02X ", pDataChar[szIndex]);
    }

    // Print a newline at the end for clean output
    printf("\n");
} // PrintBytesInHex

// End of file
