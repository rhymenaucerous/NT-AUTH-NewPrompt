/**

    @file      WinSignal.h
    @brief     Header file for console control signal handling on Windows.

**/
#pragma once

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

/**
    @brief  Initializes console control signal handling for graceful shutdown.

    @WARNING: User is responsible for closing the returned event handle.

    @retval  - HANDLE to shutdown event on success, NULL on failure.
**/
HANDLE
WinSignalInitialize();
