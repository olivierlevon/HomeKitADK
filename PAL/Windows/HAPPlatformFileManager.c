// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <stdio.h>
#include <string.h>
#include <Windows.h>

#include "HAPPlatform+Init.h"
#include "HAPPlatformFileManager.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "FileManager" };

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileManagerCreateDirectory(const char* dirPath) {
    HAPPrecondition(dirPath);

    HAPError err;

    char path[MAX_PATH];

    // Duplicate string, as each path component needs to be modified to be NULL-terminated.
    err = HAPStringWithFormat(path, sizeof path, "%s", dirPath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Directory path too long: %s", dirPath);
        return kHAPError_Unknown;
    }

    // Normalize path separators
    for (char* p = path; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Create parent directories
    for (char *start = path, *end = strchr(start, '\\'); end; start = end + 1, end = strchr(start, '\\')) {
        if (start == end) {
            // Root, or double separator
            continue;
        }

        // Skip drive letter (e.g., "C:")
        if (end - start == 2 && start[1] == ':') {
            continue;
        }

        // Replace separator with \0 temporarily. Create directory. Restore back.
        *end = '\0';
        BOOL success = CreateDirectoryA(path, NULL);
        DWORD error = GetLastError();
        *end = '\\';

        if (!success && error != ERROR_ALREADY_EXISTS) {
            *end = '\0';
            HAPLogError(&logObject, "CreateDirectory %s failed: %lu.", path, error);
            *end = '\\';
            return kHAPError_Unknown;
        }
    }

    // Create directory
    BOOL success = CreateDirectoryA(path, NULL);
    if (!success) {
        DWORD error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS) {
            HAPLogError(&logObject, "CreateDirectory %s failed: %lu.", path, error);
            return kHAPError_Unknown;
        }
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileManagerWriteFile(const char* filePath, const void* _Nullable bytes, size_t numBytes)
        HAP_DIAGNOSE_ERROR(!bytes && numBytes, "empty buffer cannot have a length") {
    HAPPrecondition(filePath);
    HAPAssert(bytes || !numBytes);

    HAPError err;

    char targetDirPath[MAX_PATH];
    err = HAPStringWithFormat(targetDirPath, sizeof targetDirPath, "%s", filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Not enough resources to copy string: %s", filePath);
        return kHAPError_Unknown;
    }

    // Normalize path separators
    for (char* p = targetDirPath; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Get split relative file path and dir path
    const char* relativeFilePath = filePath;
    {
        const size_t filePathLength = HAPStringGetNumBytes(filePath);
        HAPPrecondition(filePathLength);

        size_t dirPathLength = 0;
        for (size_t i = 0; i < filePathLength; ++i) {
            if (targetDirPath[i] == '\\') {
                dirPathLength = i;
                relativeFilePath = &filePath[i + 1];
            }
        }
        if (dirPathLength > 0) {
            targetDirPath[dirPathLength] = '\0';
        } else {
            targetDirPath[0] = '.';
            targetDirPath[1] = '\0';
        }
    }

    // Create directory
    err = HAPPlatformFileManagerCreateDirectory(targetDirPath);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPLogError(&logObject, "Create directory %s failed.", targetDirPath);
        return err;
    }

    // Create the filename of the temporary file
    char tmpPath[MAX_PATH];
    err = HAPStringWithFormat(tmpPath, sizeof tmpPath, "%s\\%s-tmp", targetDirPath, relativeFilePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Not enough resources to get path: %s\\%s-tmp", targetDirPath, relativeFilePath);
        return kHAPError_Unknown;
    }

    // Normalize path
    for (char* p = tmpPath; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Open the tempfile
    HANDLE tmpFileHandle = CreateFileA(
            tmpPath,
            GENERIC_WRITE,
            0,                        // No sharing
            NULL,                     // Default security
            CREATE_ALWAYS,            // Overwrite if exists
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (tmpFileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "CreateFile %s failed: %lu.", tmpPath, error);
        return kHAPError_Unknown;
    }

    // Write to temporary file
    if (bytes) {
        DWORD totalWritten = 0;
        while (totalWritten < numBytes) {
            DWORD toWrite = (DWORD)((numBytes - totalWritten) > 0xFFFFFFFF ? 0xFFFFFFFF : (numBytes - totalWritten));
            DWORD written = 0;

            if (!WriteFile(tmpFileHandle, &((const uint8_t*) bytes)[totalWritten], toWrite, &written, NULL)) {
                DWORD error = GetLastError();
                HAPLogError(&logObject, "WriteFile to %s failed: %lu.", tmpPath, error);
                CloseHandle(tmpFileHandle);
                DeleteFileA(tmpPath);
                return kHAPError_Unknown;
            }

            if (written == 0) {
                HAPLogError(&logObject, "WriteFile to %s returned 0 bytes written.", tmpPath);
                CloseHandle(tmpFileHandle);
                DeleteFileA(tmpPath);
                return kHAPError_Unknown;
            }

            totalWritten += written;
        }
    }

    // Flush and close the temporary file
    if (!FlushFileBuffers(tmpFileHandle)) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "FlushFileBuffers of %s failed: %lu.", tmpPath, error);
    }
    CloseHandle(tmpFileHandle);

    // Construct final file path
    char finalPath[MAX_PATH];
    err = HAPStringWithFormat(finalPath, sizeof finalPath, "%s\\%s", targetDirPath, relativeFilePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Not enough resources to get final path.");
        DeleteFileA(tmpPath);
        return kHAPError_Unknown;
    }

    // Normalize path
    for (char* p = finalPath; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Rename file (atomic operation on Windows)
    if (!MoveFileExA(tmpPath, finalPath, MOVEFILE_REPLACE_EXISTING)) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "MoveFileEx from %s to %s failed: %lu.", tmpPath, finalPath, error);
        DeleteFileA(tmpPath);
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileManagerReadFile(
        const char* filePath,
        void* _Nullable bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* valid) {
    HAPPrecondition(filePath);
    HAPPrecondition(!maxBytes || bytes);
    HAPPrecondition((bytes == NULL) == (numBytes == NULL));
    HAPPrecondition(valid);

    *valid = false;

    char path[MAX_PATH];
    HAPError err = HAPStringWithFormat(path, sizeof path, "%s", filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Path too long: %s", filePath);
        return kHAPError_Unknown;
    }

    // Normalize path
    for (char* p = path; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    HANDLE fileHandle = CreateFileA(
            path,
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            return kHAPError_None;
        }
        HAPLogError(&logObject, "CreateFile %s failed: %lu.", path, error);
        return kHAPError_Unknown;
    }

    *valid = true;

    if (bytes) {
        HAPAssert(numBytes);

        DWORD totalRead = 0;
        while (totalRead < maxBytes) {
            DWORD toRead = (DWORD)((maxBytes - totalRead) > 0xFFFFFFFF ? 0xFFFFFFFF : (maxBytes - totalRead));
            DWORD read = 0;

            if (!ReadFile(fileHandle, &((uint8_t*) bytes)[totalRead], toRead, &read, NULL)) {
                DWORD error = GetLastError();
                HAPLogError(&logObject, "ReadFile %s failed: %lu.", path, error);
                CloseHandle(fileHandle);
                return kHAPError_Unknown;
            }

            if (read == 0) {
                break; // EOF
            }

            totalRead += read;
        }
        *numBytes = totalRead;
    }

    CloseHandle(fileHandle);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileManagerRemoveFile(const char* filePath) {
    HAPPrecondition(filePath);

    char path[MAX_PATH];
    HAPError err = HAPStringWithFormat(path, sizeof path, "%s", filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(&logObject, "Path too long: %s", filePath);
        return kHAPError_Unknown;
    }

    // Normalize path
    for (char* p = path; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    // Test if file exists
    DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            // File does not exist
            return kHAPError_None;
        }
        HAPLogError(&logObject, "GetFileAttributes %s failed: %lu.", path, error);
        HAPFatalError();
    }

    // Verify it's a file, not a directory
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        HAPLogError(&logObject, "Path %s is a directory, not a file.", path);
        HAPFatalError();
    }

    // Remove file
    if (!DeleteFileA(path)) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            // File does not exist
            return kHAPError_None;
        }
        HAPLogError(&logObject, "DeleteFile %s failed: %lu.", path, error);
        return kHAPError_Unknown;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileManagerNormalizePath(const char* path, char* bytes, size_t maxBytes) {
    HAPPrecondition(path);
    HAPPrecondition(bytes);

    HAPLogDebug(&logObject, "%s: Expanding '%s'", __func__, path);

    // On Windows, expand environment variables
    DWORD len = ExpandEnvironmentStringsA(path, bytes, (DWORD) maxBytes);
    if (len == 0) {
        DWORD error = GetLastError();
        HAPLogError(&logObject, "%s: ExpandEnvironmentStrings failed: %lu.", __func__, error);
        return kHAPError_Unknown;
    }

    if (len > maxBytes) {
        HAPLogError(&logObject, "%s: Target buffer too small (got: %zu, needs: %lu).", __func__, maxBytes, len);
        return kHAPError_OutOfResources;
    }

    // Normalize path separators to backslashes
    for (char* p = bytes; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    return kHAPError_None;
}
