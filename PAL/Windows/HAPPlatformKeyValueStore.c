// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <stdio.h>
#include <Windows.h>

#include "HAPPlatform+Init.h"
#include "HAPPlatformKeyValueStore+Init.h"
#include "HAPPlatformFileManager.h"

#ifndef MAX_PATH
#define MAX_PATH 260
#endif

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "KeyValueStore" };

/**
 * Enumerates directory @p dir, calling @p body on each directory entry.
 *
 * @param      dir                  Path to directory to scan.
 * @param      body                 Function to call on each directory entry.
 * @param      ctx                  Context that is passed to @p body.
 *
 * @return 0                        If successful.
 * @return -1                       Otherwise.
 */
HAP_RESULT_USE_CHECK
static int
        enumdir(const char* dir,
                int (*body)(void* ctx, const char* dir, const char* fileName, bool* cont),
                void* ctx) {
    HAPPrecondition(dir);
    HAPPrecondition(body);

    char searchPath[MAX_PATH];
    HAPError err = HAPStringWithFormat(searchPath, sizeof searchPath, "%s\\*", dir);
    if (err) {
        HAPLogError(&logObject, "Path too long for directory enumeration: %s", dir);
        return -1;
    }

    // Normalize path
    for (char* p = searchPath; *p; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) {
            // Directory doesn't exist or is empty
            return 0;
        }
        HAPLogError(&logObject, "FindFirstFile %s failed: %lu.", searchPath, error);
        return -1;
    }

    // Iterate through directory
    bool cont = true;
    do {
        // Invoke callback
        int e = body(ctx, dir, findData.cFileName, &cont);
        if (e) {
            FindClose(hFind);
            return -1;
        }

        if (!cont) {
            break;
        }
    } while (FindNextFileA(hFind, &findData));

    DWORD error = GetLastError();
    FindClose(hFind);

    if (error != ERROR_NO_MORE_FILES) {
        HAPLogError(&logObject, "FindNextFile %s failed: %lu.", searchPath, error);
        return -1;
    }

    return 0;
}

void HAPPlatformKeyValueStoreCreate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        const HAPPlatformKeyValueStoreOptions* options) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(options);
    HAPPrecondition(options->rootDirectory);

    HAPLogDebug(&logObject, "Storage configuration: keyValueStore = %lu", (unsigned long) sizeof *keyValueStore);

    keyValueStore->rootDirectory = options->rootDirectory;
}

/**
 * Gets the file path under which data for a specified key is stored.
 *
 * @param      keyValueStore        Key-value store.
 * @param      domain               Domain.
 * @param      key                  Key.
 * @param[out] filePath             File path for the domain / key. NULL-terminated.
 * @param      maxFilePathLength    Maximum length that the filePath buffer may hold.
 *
 * @return kHAPError_None           If successful.
 * @return kHAPError_OutOfResources If @p cap not large enough.
 */
HAP_RESULT_USE_CHECK
static HAPError GetFilePath(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        char* filePath,
        size_t maxFilePathLength) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);
    HAPPrecondition(filePath);

    HAPError err;

    HAPAssert(sizeof domain == sizeof(uint8_t));
    HAPAssert(sizeof key == sizeof(uint8_t));
    err = HAPStringWithFormat(filePath, maxFilePathLength, "%s\\%02X.%02X", keyValueStore->rootDirectory, domain, key);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        HAPLogError(
                &logObject,
                "Not enough resources to get path: %s\\%02X.%02X",
                keyValueStore->rootDirectory,
                domain,
                key);
        return kHAPError_OutOfResources;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreGet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        void* _Nullable bytes,
        size_t maxBytes,
        size_t* _Nullable numBytes,
        bool* found) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);
    HAPPrecondition(!maxBytes || bytes);
    HAPPrecondition((bytes == NULL) == (numBytes == NULL));
    HAPPrecondition(found);

    HAPError err;

    // Get file name
    char filePath[MAX_PATH];
    err = GetFilePath(keyValueStore, domain, key, filePath, sizeof filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        return kHAPError_Unknown;
    }

    return HAPPlatformFileManagerReadFile(filePath, bytes, maxBytes, numBytes, found);
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreSet(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        const void* bytes,
        size_t numBytes) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);
    HAPPrecondition(bytes);

    HAPError err;

    char filePath[MAX_PATH];

    // Get file name
    err = GetFilePath(keyValueStore, domain, key, filePath, sizeof filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        return kHAPError_Unknown;
    }

    // Write the KVS file
    err = HAPPlatformFileManagerWriteFile(filePath, bytes, numBytes);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        return err;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreRemove(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);

    HAPError err;

    char filePath[MAX_PATH];

    // Get file name
    err = GetFilePath(keyValueStore, domain, key, filePath, sizeof filePath);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        return kHAPError_Unknown;
    }

    // Remove file
    err = HAPPlatformFileManagerRemoveFile(filePath);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        return err;
    }

    return kHAPError_None;
}

/**
 * enumdir context.
 */
typedef struct {
    /**
     * Key-value store.
     */
    HAPPlatformKeyValueStoreRef keyValueStore;

    /**
     * Domain to enumerate.
     */
    HAPPlatformKeyValueStoreDomain domain;

    /**
     * Client callback.
     */
    HAPPlatformKeyValueStoreEnumerateCallback body;

    /**
     * Client context.
     */
    void* _Nullable context;
} EnumdirCallbackContext;

HAP_RESULT_USE_CHECK
static int EnumdirCallback(void* ctx, const char* dir, const char* fileName, bool* cont) {
    EnumdirCallbackContext* arguments = ctx;
    HAPPrecondition(arguments);
    HAPPrecondition(arguments->keyValueStore);
    HAPPrecondition(arguments->body);
    HAPPrecondition(dir);
    HAPPrecondition(fileName);
    HAPPrecondition(cont);

    HAPError err;

    // Parse file name
    if (HAPStringAreEqual(fileName, ".")) {
        return 0;
    }
    if (HAPStringAreEqual(fileName, "..")) {
        return 0;
    }
    unsigned int domain;
    unsigned int key;
    int end;
    int n = sscanf(fileName, "%2X.%2X%n", &domain, &key, &end);
    if (n != 2 || (size_t) end != HAPStringGetNumBytes(fileName)) {
        HAPLog(&logObject, "Skipping unexpected file in key-value store directory: %s", fileName);
        return 0;
    }
    HAPAssert(sizeof(HAPPlatformKeyValueStoreDomain) == sizeof(uint8_t));
    if (domain > UINT8_MAX) {
        HAPLog(&logObject, "Skipping file with too large domain in key-value store directory: %s", fileName);
        return 0;
    }
    HAPAssert(sizeof(HAPPlatformKeyValueStoreKey) == sizeof(uint8_t));
    if (key > UINT8_MAX) {
        HAPLog(&logObject, "Skipping file with too large key in key-value store directory: %s", fileName);
        return 0;
    }

    // Check domain
    if ((HAPPlatformKeyValueStoreDomain) domain != arguments->domain) {
        return 0;
    }

    // Invoke callback
    err = arguments->body(
            arguments->context,
            arguments->keyValueStore,
            (HAPPlatformKeyValueStoreDomain) domain,
            (HAPPlatformKeyValueStoreKey) key,
            cont);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        return -1;
    }
    return 0;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStoreEnumerate(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreEnumerateCallback callback,
        void* _Nullable context) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);
    HAPPrecondition(callback);

    int e =
            enumdir(keyValueStore->rootDirectory,
                    EnumdirCallback,
                    &(EnumdirCallbackContext) {
                            .keyValueStore = keyValueStore,
                            .domain = domain,
                            .body = callback,
                            .context = context,
                    });
    if (e) {
        HAPAssert(e == -1);
        return kHAPError_Unknown;
    }
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
static HAPError PurgeDomainEnumerateCallback(
        void* _Nullable context HAP_UNUSED,
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain,
        HAPPlatformKeyValueStoreKey key,
        bool* shouldContinue) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);
    HAPPrecondition(shouldContinue);

    HAPError err;

    err = HAPPlatformKeyValueStoreRemove(keyValueStore, domain, key);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        return err;
    }

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformKeyValueStorePurgeDomain(
        HAPPlatformKeyValueStoreRef keyValueStore,
        HAPPlatformKeyValueStoreDomain domain) {
    HAPPrecondition(keyValueStore);
    HAPPrecondition(keyValueStore->rootDirectory);

    HAPError err;

    err = HAPPlatformKeyValueStoreEnumerate(keyValueStore, domain, PurgeDomainEnumerateCallback, NULL);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        return err;
    }

    return kHAPError_None;
}
