// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HAPPlatformSystemCommand.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "SystemCommand" };

HAP_RESULT_USE_CHECK
HAPError HAPPlatformSystemCommandRun(
        char* _Nullable const command[_Nonnull],
        char* bytes,
        size_t maxBytes,
        size_t* numBytes) {
    HAPPrecondition(command);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes);

    return HAPPlatformSystemCommandRunWithEnvironment(command, /* environment: */ NULL, bytes, maxBytes, numBytes);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformSystemCommandRunWithEnvironment(
        char* _Nullable const command[_Nonnull],
        char* _Nullable const environment[_Nullable],
        char* bytes,
        size_t maxBytes,
        size_t* numBytes) {
    HAPPrecondition(command);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes);

    // Create pipe for reading child process output
    HANDLE hReadPipe, hWritePipe;
    SECURITY_ATTRIBUTES sa = {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .bInheritHandle = TRUE,
        .lpSecurityDescriptor = NULL
    };

    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "%s: CreatePipe failed: %lu.", __func__, error);
        return kHAPError_Unknown;
    }

    // Ensure the read handle to the pipe for STDOUT is not inherited
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "%s: SetHandleInformation failed: %lu.", __func__, error);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return kHAPError_Unknown;
    }

    // Build command line
    char cmdLine[8192] = {0};
    size_t offset = 0;
    for (int i = 0; command[i] != NULL; i++) {
        if (i > 0) {
            cmdLine[offset++] = ' ';
        }
        // Quote arguments with spaces
        bool hasSpace = strchr(command[i], ' ') != NULL;
        if (hasSpace) {
            cmdLine[offset++] = '"';
        }
        size_t len = strlen(command[i]);
        memcpy(&cmdLine[offset], command[i], len);
        offset += len;
        if (hasSpace) {
            cmdLine[offset++] = '"';
        }
    }
    cmdLine[offset] = '\0';

    // Setup process startup info
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Create environment block if provided
    char* envBlock = NULL;
    if (environment) {
        // Calculate size needed for environment block
        size_t totalSize = 0;
        for (int i = 0; environment[i] != NULL; i++) {
            totalSize += strlen(environment[i]) + 1;
        }
        totalSize += 1; // Final null terminator

        envBlock = (char*)malloc(totalSize);
        if (envBlock) {
            char* p = envBlock;
            for (int i = 0; environment[i] != NULL; i++) {
                size_t len = strlen(environment[i]);
                memcpy(p, environment[i], len);
                p += len;
                *p++ = '\0';
            }
            *p = '\0'; // Final terminator
        }
    }

    // Create process
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcessA(
            NULL,           // Application name
            cmdLine,        // Command line
            NULL,           // Process security attributes
            NULL,           // Thread security attributes
            TRUE,           // Inherit handles
            0,              // Creation flags
            envBlock,       // Environment
            NULL,           // Current directory
            &si,            // Startup info
            &pi);           // Process info

    if (envBlock) {
        free(envBlock);
    }

    if (!success) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "%s: CreateProcess failed: %lu.", __func__, error);
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return kHAPError_Unknown;
    }

    // Close write end of pipe in parent
    CloseHandle(hWritePipe);

    // Read output from child process
    *numBytes = 0;
    bool bufferTooSmall = false;

    while (*numBytes < maxBytes) {
        DWORD bytesRead;
        DWORD available = maxBytes - *numBytes;
        if (!ReadFile(hReadPipe, &bytes[*numBytes], available, &bytesRead, NULL)) {
            DWORD error = GetLastError();
            if (error != ERROR_BROKEN_PIPE) {
                HAPLogError(&logObject, "%s: ReadFile failed: %lu.", __func__, error);
            }
            break;
        }

        if (bytesRead == 0) {
            break; // EOF
        }

        *numBytes += bytesRead;

        // Check if buffer is full
        if (*numBytes == maxBytes) {
            // Try to read one more byte to check if there's more data
            char tempByte;
            DWORD tempRead;
            if (ReadFile(hReadPipe, &tempByte, 1, &tempRead, NULL) && tempRead > 0) {
                bufferTooSmall = true;
                break;
            }
        }
    }

    CloseHandle(hReadPipe);

    // Wait for child process to exit
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode)) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "%s: GetExitCodeProcess failed: %lu.", __func__, error);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return kHAPError_Unknown;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Check results
    if (exitCode != 0) {
        HAPLogInfo(&logObject, "%s: Process exited with status %lu.", __func__, exitCode);
        return kHAPError_Unknown;
    }

    if (bufferTooSmall) {
        HAPLogInfo(&logObject, "%s: Buffer too small to store result.", __func__);
        return kHAPError_OutOfResources;
    }

    return kHAPError_None;
}
