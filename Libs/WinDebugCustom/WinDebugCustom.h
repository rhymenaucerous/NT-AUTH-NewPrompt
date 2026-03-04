/**

    @file      WinDebugCustom.h
    @brief     This header file contains custom debugging macros for Windows
               applications. The library does not contain a source file, but
               this header can be included in projects to provide enhanced
               debugging capabilities during development.

**/
#pragma once

// Standard C includes
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Other Libraries required for debugging
#include <stdio.h>

#ifdef _DEBUG
#pragma warning(disable : 4996) // Disable warning C4996 (deprecated functions)
#define DEBUG_PRINT(fmt, ...)                      \
    do                                             \
    {                                              \
        fprintf(stderr,                            \
                "DEBUG: %s(): Line %d: " fmt "\n", \
                __func__,                          \
                __LINE__,                          \
                __VA_ARGS__);                      \
    } while (0)
#define DEBUG_ERROR(fmt, ...)                                          \
    do                                                                 \
    {                                                                  \
        DWORD error_code = GetLastError();                             \
        char  error_message[256];                                      \
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM                      \
                           | FORMAT_MESSAGE_IGNORE_INSERTS,            \
                       NULL,                                           \
                       error_code,                                     \
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),      \
                       error_message,                                  \
                       sizeof(error_message),                          \
                       NULL);                                          \
        fprintf(stderr,                                                \
                "DEBUG: %s(): Line %d:\nError %lu: %sNote: " fmt "\n", \
                __func__,                                              \
                __LINE__,                                              \
                error_code,                                            \
                error_message,                                         \
                __VA_ARGS__);                                          \
    } while (0)
#define DEBUG_ERROR_SUPPLIED(error_code, fmt, ...)                     \
    do                                                                 \
    {                                                                  \
        char error_message[256];                                       \
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM                      \
                           | FORMAT_MESSAGE_IGNORE_INSERTS,            \
                       NULL,                                           \
                       error_code,                                     \
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),      \
                       error_message,                                  \
                       sizeof(error_message),                          \
                       NULL);                                          \
        fprintf(stderr,                                                \
                "DEBUG: %s(): Line %d:\nError %lu: %sNote: " fmt "\n", \
                __func__,                                              \
                __LINE__,                                              \
                error_code,                                            \
                error_message,                                         \
                __VA_ARGS__);                                          \
    } while (0)
#define DEBUG_WSAERROR(fmt, ...)                                      \
    do                                                                \
    {                                                                 \
        int  wsa_error_code = WSAGetLastError();                      \
        char error_message[256];                                      \
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM                     \
                           | FORMAT_MESSAGE_IGNORE_INSERTS,           \
                       NULL,                                          \
                       wsa_error_code,                                \
                       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),     \
                       error_message,                                 \
                       sizeof(error_message),                         \
                       NULL);                                         \
        fprintf(stderr,                                               \
                "DEBUG: %s(): Line %d:\nError %d: %sNote: " fmt "\n", \
                __func__,                                             \
                __LINE__,                                             \
                wsa_error_code,                                       \
                error_message,                                        \
                __VA_ARGS__);                                         \
    } while (0)
#define CUSTOM_PRINT(fmt, ...)                      \
    do                                              \
    {                                               \
        fprintf(stderr,                             \
                "CUSTOM: %s(): Line %d: " fmt "\n", \
                __func__,                           \
                __LINE__,                           \
                __VA_ARGS__);                       \
    } while (0)
#define DEBUG_NTSTATUS(_ntstatus, fmt, ...)                               \
    do                                                                    \
    {                                                                     \
        NTSTATUS __dbg_nt    = (NTSTATUS)(_ntstatus);                     \
        DWORD    __dbg_win32 = 0;                                         \
        LPSTR    __dbg_msg   = NULL;                                      \
        HMODULE  __dbg_hnt   = GetModuleHandleA("ntdll.dll");             \
        typedef ULONG(WINAPI *PFN_RtlNtStatusToDosError)(NTSTATUS);       \
        PFN_RtlNtStatusToDosError __dbg_pRtl = NULL;                      \
                                                                          \
        if (__dbg_hnt)                                                    \
        {                                                                 \
            __dbg_pRtl = (PFN_RtlNtStatusToDosError)GetProcAddress(       \
                __dbg_hnt, "RtlNtStatusToDosError");                      \
        }                                                                 \
                                                                          \
        /* Try NTSTATUS -> Win32 error -> system message */               \
        if (__dbg_pRtl)                                                   \
        {                                                                 \
            __dbg_win32 = (DWORD)__dbg_pRtl(__dbg_nt);                    \
            if (0 != __dbg_win32)                                         \
            {                                                             \
                FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER             \
                                   | FORMAT_MESSAGE_FROM_SYSTEM           \
                                   | FORMAT_MESSAGE_IGNORE_INSERTS,       \
                               NULL,                                      \
                               __dbg_win32,                               \
                               MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
                               (LPSTR) & __dbg_msg,                       \
                               0,                                         \
                               NULL);                                     \
            }                                                             \
        }                                                                 \
                                                                          \
        /* If no system message, try ntdll's message table */             \
        if (NULL == __dbg_msg && NULL != __dbg_hnt)                       \
        {                                                                 \
            FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER                 \
                               | FORMAT_MESSAGE_FROM_HMODULE              \
                               | FORMAT_MESSAGE_IGNORE_INSERTS,           \
                           __dbg_hnt,                                     \
                           (DWORD)__dbg_nt,                               \
                           MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),     \
                           (LPSTR) & __dbg_msg,                           \
                           0,                                             \
                           NULL);                                         \
        }                                                                 \
                                                                          \
        /* Trim trailing CR/LF from message if present */                 \
        if (NULL != __dbg_msg)                                            \
        {                                                                 \
            size_t __dbg_len = strlen(__dbg_msg);                         \
            while (__dbg_len > 0                                          \
                   && (__dbg_msg[__dbg_len - 1] == '\n'                   \
                       || __dbg_msg[__dbg_len - 1] == '\r'))              \
            {                                                             \
                __dbg_msg[--__dbg_len] = '\0';                            \
            }                                                             \
            fprintf(stderr,                                               \
                    "DEBUG: %s(): Line %d: NTSTATUS 0x%08x: %s" fmt "\n", \
                    __func__,                                             \
                    __LINE__,                                             \
                    (unsigned)__dbg_nt,                                   \
                    __dbg_msg,                                            \
                    __VA_ARGS__);                                         \
            LocalFree(__dbg_msg);                                         \
        }                                                                 \
        else                                                              \
        {                                                                 \
            /* Fallback: just print the NTSTATUS code */                  \
            fprintf(stderr,                                               \
                    "DEBUG: %s(): Line %d: NTSTATUS 0x%08x. " fmt "\n",   \
                    __func__,                                             \
                    __LINE__,                                             \
                    (unsigned)__dbg_nt,                                   \
                    __VA_ARGS__);                                         \
        }                                                                 \
    } while (0)
#else
#define DEBUG_PRINT(fmt, ...) \
    do                        \
    {                         \
    } while (0)
#define DEBUG_ERROR(fmt, ...) \
    do                        \
    {                         \
    } while (0)
#define DEBUG_ERROR_SUPPLIED(fmt, ...) \
    do                                 \
    {                                  \
    } while (0)
#define DEBUG_WSAERROR(fmt, ...) \
    do                           \
    {                            \
    } while (0)
#define CUSTOM_PRINT(fmt, ...) \
    do                         \
    {                          \
    } while (0)
#define DEBUG_NTSTATUS(_ntstatus, fmt, ...) \
    do                                      \
    {                                       \
    } while (0)
#endif

// End of file
