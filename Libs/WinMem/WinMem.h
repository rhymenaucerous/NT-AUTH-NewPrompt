/**

    @file      WinMem.h
    @brief     Header file for custom memory management utilities on Windows.

**/
#pragma once

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Other Libraries required for debugging library
#include <stdio.h>

/**
 * @brief Securely frees memory by zeroing it before deallocation.
 *
 * @param pMem Pointer to the memory block to be freed
 * @param dwNumBytes Size of the memory block in bytes
 * @return VOID
 */
VOID ZeroingHeapFree(_In_ PVOID *ppMem, _In_ DWORD dwNumBytes);

/**
    @brief  Prints a block of memory in hex format.
    @param  pData - pointer to the data.
    @param  szNum - the number of bytes to print.
    @retval       - VOID.
**/
VOID PrintBytesInHex(PVOID pData, SIZE_T szNum);

// End of file
