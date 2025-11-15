// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// Windows implementation using WSAPoll for I/O multiplexing

#include "HAPPlatform.h"

#include <Windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "HAPPlatform+Init.h"
#include "HAPPlatformFileHandle.h"
#include "HAPPlatformLog+Init.h"
#include "HAPPlatformRunLoop+Init.h"

#pragma comment(lib, "ws2_32.lib")

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "RunLoop" };

/**
 * Internal file handle type, representing the registration of a platform-specific file descriptor.
 */
typedef struct HAPPlatformFileHandle HAPPlatformFileHandle;

/**
 * Internal file handle representation.
 */
struct HAPPlatformFileHandle {
    /**
     * Platform-specific file descriptor (SOCKET on Windows).
     */
    SOCKET fileDescriptor;

    /**
     * Set of file handle events on which the callback shall be invoked.
     */
    HAPPlatformFileHandleEvent interests;

    /**
     * Function to call when one or more events occur on the given file descriptor.
     */
    HAPPlatformFileHandleCallback callback;

    /**
     * The context parameter given to the HAPPlatformFileHandleRegister function.
     */
    void* _Nullable context;

    /**
     * Previous file handle in linked list.
     */
    HAPPlatformFileHandle* _Nullable prevFileHandle;

    /**
     * Next file handle in linked list.
     */
    HAPPlatformFileHandle* _Nullable nextFileHandle;

    /**
     * Flag indicating whether the platform-specific file descriptor is registered with an I/O multiplexer or not.
     */
    bool isAwaitingEvents;
};

/**
 * Internal timer type.
 */
typedef struct HAPPlatformTimer HAPPlatformTimer;

/**
 * Internal timer representation.
 */
struct HAPPlatformTimer {
    /**
     * Deadline at which the timer expires.
     */
    HAPTime deadline;

    /**
     * Callback that is invoked when the timer expires.
     */
    HAPPlatformTimerCallback callback;

    /**
     * The context parameter given to the HAPPlatformTimerRegister function.
     */
    void* _Nullable context;

    /**
     * Next timer in linked list.
     */
    HAPPlatformTimer* _Nullable nextTimer;
};

/**
 * Run loop state.
 */
HAP_ENUM_BEGIN(uint8_t, HAPPlatformRunLoopState) {
    kHAPPlatformRunLoopState_Idle,
    kHAPPlatformRunLoopState_Running,
    kHAPPlatformRunLoopState_Stopping
} HAP_ENUM_END(uint8_t, HAPPlatformRunLoopState);

static struct {
    HAPPlatformFileHandle fileHandleSentinel;
    HAPPlatformFileHandle* _Nullable fileHandles;
    HAPPlatformFileHandle* _Nullable fileHandleCursor;
    HAPPlatformTimer* _Nullable timers;

    // Windows event object for waking up the run loop
    HANDLE wakeEvent;

    // Self-pipe equivalent using socket pair
    volatile SOCKET selfPipeSocket0;  // Receive
    volatile SOCKET selfPipeSocket1;  // Send

    HAP_ALIGNAS(8)
    char selfPipeBytes[sizeof(HAPPlatformRunLoopCallback) + 1 + UINT8_MAX];
    size_t numSelfPipeBytes;

    HAPPlatformFileHandleRef selfPipeFileHandle;
    HAPPlatformRunLoopState state;

    // Winsock initialized flag
    bool winsockInitialized;
} runLoop = {
    .fileHandleSentinel = {
        .fileDescriptor = INVALID_SOCKET,
        .interests = { .isReadyForReading = false, .isReadyForWriting = false, .hasErrorConditionPending = false },
        .callback = NULL,
        .context = NULL,
        .prevFileHandle = &runLoop.fileHandleSentinel,
        .nextFileHandle = &runLoop.fileHandleSentinel,
        .isAwaitingEvents = false
    },
    .fileHandles = &runLoop.fileHandleSentinel,
    .fileHandleCursor = &runLoop.fileHandleSentinel,
    .timers = NULL,
    .wakeEvent = NULL,
    .selfPipeSocket0 = INVALID_SOCKET,
    .selfPipeSocket1 = INVALID_SOCKET,
    .winsockInitialized = false
};

HAP_RESULT_USE_CHECK
HAPError HAPPlatformFileHandleRegister(
        HAPPlatformFileHandleRef* fileHandle_,
        int fileDescriptor,
        HAPPlatformFileHandleEvent interests,
        HAPPlatformFileHandleCallback callback,
        void* _Nullable context) {
    HAPPrecondition(fileHandle_);

    HAPPlatformFileHandle* fileHandle = calloc(1, sizeof(HAPPlatformFileHandle));
    if (!fileHandle) {
        HAPLog(&logObject, "Cannot allocate more file handles.");
        *fileHandle_ = 0;
        return kHAPError_OutOfResources;
    }
    fileHandle->fileDescriptor = (SOCKET)fileDescriptor;
    fileHandle->interests = interests;
    fileHandle->callback = callback;
    fileHandle->context = context;
    fileHandle->prevFileHandle = runLoop.fileHandles->prevFileHandle;
    fileHandle->nextFileHandle = runLoop.fileHandles;
    fileHandle->isAwaitingEvents = false;
    runLoop.fileHandles->prevFileHandle->nextFileHandle = fileHandle;
    runLoop.fileHandles->prevFileHandle = fileHandle;

    *fileHandle_ = (HAPPlatformFileHandleRef) fileHandle;
    return kHAPError_None;
}

void HAPPlatformFileHandleUpdateInterests(
        HAPPlatformFileHandleRef fileHandle_,
        HAPPlatformFileHandleEvent interests,
        HAPPlatformFileHandleCallback callback,
        void* _Nullable context) {
    HAPPrecondition(fileHandle_);
    HAPPlatformFileHandle* fileHandle = (HAPPlatformFileHandle * _Nonnull) fileHandle_;

    fileHandle->interests = interests;
    fileHandle->callback = callback;
    fileHandle->context = context;
}

void HAPPlatformFileHandleDeregister(HAPPlatformFileHandleRef fileHandle_) {
    HAPPrecondition(fileHandle_);
    HAPPlatformFileHandle* fileHandle = (HAPPlatformFileHandle * _Nonnull) fileHandle_;

    HAPPrecondition(fileHandle->prevFileHandle);
    HAPPrecondition(fileHandle->nextFileHandle);

    if (fileHandle == runLoop.fileHandleCursor) {
        runLoop.fileHandleCursor = fileHandle->nextFileHandle;
    }

    fileHandle->prevFileHandle->nextFileHandle = fileHandle->nextFileHandle;
    fileHandle->nextFileHandle->prevFileHandle = fileHandle->prevFileHandle;

    fileHandle->fileDescriptor = INVALID_SOCKET;
    fileHandle->interests.isReadyForReading = false;
    fileHandle->interests.isReadyForWriting = false;
    fileHandle->interests.hasErrorConditionPending = false;
    fileHandle->callback = NULL;
    fileHandle->context = NULL;
    fileHandle->nextFileHandle = NULL;
    fileHandle->prevFileHandle = NULL;
    fileHandle->isAwaitingEvents = false;
    HAPPlatformFreeSafe(fileHandle);
}

static void ProcessSelectedFileHandles(WSAPOLLFD* pollFds, int numFds) {
    HAPPrecondition(pollFds);

    runLoop.fileHandleCursor = runLoop.fileHandles->nextFileHandle;
    while (runLoop.fileHandleCursor != runLoop.fileHandles) {
        HAPPlatformFileHandle* fileHandle = runLoop.fileHandleCursor;
        runLoop.fileHandleCursor = fileHandle->nextFileHandle;

        if (fileHandle->isAwaitingEvents) {
            HAPAssert(fileHandle->fileDescriptor != INVALID_SOCKET);
            fileHandle->isAwaitingEvents = false;

            if (fileHandle->callback) {
                // Find corresponding poll entry
                for (int i = 0; i < numFds; i++) {
                    if (pollFds[i].fd == fileHandle->fileDescriptor) {
                        HAPPlatformFileHandleEvent fileHandleEvents;
                        fileHandleEvents.isReadyForReading = fileHandle->interests.isReadyForReading &&
                                                            (pollFds[i].revents & POLLIN);
                        fileHandleEvents.isReadyForWriting = fileHandle->interests.isReadyForWriting &&
                                                            (pollFds[i].revents & POLLOUT);
                        fileHandleEvents.hasErrorConditionPending = fileHandle->interests.hasErrorConditionPending &&
                                                                    (pollFds[i].revents & (POLLERR | POLLHUP));

                        if (fileHandleEvents.isReadyForReading || fileHandleEvents.isReadyForWriting ||
                            fileHandleEvents.hasErrorConditionPending) {
                            fileHandle->callback((HAPPlatformFileHandleRef) fileHandle, fileHandleEvents, fileHandle->context);
                        }
                        break;
                    }
                }
            }
        }
    }
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformTimerRegister(
        HAPPlatformTimerRef* timer_,
        HAPTime deadline,
        HAPPlatformTimerCallback callback,
        void* _Nullable context) {
    HAPPrecondition(timer_);
    HAPPlatformTimer* _Nullable* newTimer = (HAPPlatformTimer * _Nullable*) timer_;
    HAPPrecondition(callback);

    *newTimer = calloc(1, sizeof(HAPPlatformTimer));
    if (!*newTimer) {
        HAPLog(&logObject, "Cannot allocate more timers.");
        return kHAPError_OutOfResources;
    }
    (*newTimer)->deadline = deadline ? deadline : 1;
    (*newTimer)->callback = callback;
    (*newTimer)->context = context;

    for (HAPPlatformTimer* _Nullable* nextTimer = &runLoop.timers;; nextTimer = &(*nextTimer)->nextTimer) {
        if (!*nextTimer) {
            (*newTimer)->nextTimer = NULL;
            *nextTimer = *newTimer;
            break;
        }
        if ((*nextTimer)->deadline > deadline) {
            (*newTimer)->nextTimer = *nextTimer;
            *nextTimer = *newTimer;
            break;
        }
    }

    return kHAPError_None;
}

void HAPPlatformTimerDeregister(HAPPlatformTimerRef timer_) {
    HAPPrecondition(timer_);
    HAPPlatformTimer* timer = (HAPPlatformTimer*) timer_;

    for (HAPPlatformTimer* _Nullable* nextTimer = &runLoop.timers; *nextTimer; nextTimer = &(*nextTimer)->nextTimer) {
        if (*nextTimer == timer) {
            *nextTimer = timer->nextTimer;
            HAPPlatformFreeSafe(timer);
            return;
        }
    }

    HAPFatalError();
}

static void ProcessExpiredTimers(void) {
    HAPTime now = HAPPlatformClockGetCurrent();

    while (runLoop.timers) {
        if (runLoop.timers->deadline > now) {
            break;
        }

        HAPPlatformTimer* expiredTimer = runLoop.timers;
        runLoop.timers = runLoop.timers->nextTimer;

        expiredTimer->callback((HAPPlatformTimerRef) expiredTimer, expiredTimer->context);

        HAPPlatformFreeSafe(expiredTimer);
    }
}

static void ClosePipe(SOCKET socket0, SOCKET socket1) {
    if (socket0 != INVALID_SOCKET) {
        HAPLogDebug(&logObject, "closesocket(%llu);", (unsigned long long)socket0);
        closesocket(socket0);
    }
    if (socket1 != INVALID_SOCKET) {
        HAPLogDebug(&logObject, "closesocket(%llu);", (unsigned long long)socket1);
        closesocket(socket1);
    }
}

static void SelfPipeHandleCallback(
        HAPPlatformFileHandleRef fileHandle HAP_UNUSED,
        HAPPlatformFileHandleEvent fileHandleEvents,
        void* _Nullable context HAP_UNUSED) {
    if (fileHandleEvents.isReadyForReading) {
        char buffer[256];
        int numBytes = recv(runLoop.selfPipeSocket0, buffer, sizeof buffer, 0);
        if (numBytes == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
                HAPLogError(&logObject, "recv from self-pipe failed: %d.", error);
            }
        }

        if (runLoop.numSelfPipeBytes > 0) {
            HAPPlatformRunLoopCallback callback = *(HAPPlatformRunLoopCallback*) &runLoop.selfPipeBytes[0];
            uint8_t numContextBytes = runLoop.selfPipeBytes[sizeof callback];

            HAP_ALIGNAS(8) char contextBytes[UINT8_MAX];
            if (numContextBytes) {
                HAPRawBufferCopyBytes(contextBytes, &runLoop.selfPipeBytes[sizeof callback + 1], numContextBytes);
            }

            runLoop.numSelfPipeBytes = 0;

            callback(contextBytes, numContextBytes);
        }
    }
}

static HAPError CreateSelfPipe(void) {
    // Create a socket pair for self-pipe on Windows
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        HAPLogError(&logObject, "socket() failed: %d", WSAGetLastError());
        return kHAPError_Unknown;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;  // Let system choose port

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        HAPLogError(&logObject, "bind() failed: %d", WSAGetLastError());
        closesocket(listener);
        return kHAPError_Unknown;
    }

    int addrLen = sizeof(addr);
    if (getsockname(listener, (struct sockaddr*)&addr, &addrLen) == SOCKET_ERROR) {
        HAPLogError(&logObject, "getsockname() failed: %d", WSAGetLastError());
        closesocket(listener);
        return kHAPError_Unknown;
    }

    if (listen(listener, 1) == SOCKET_ERROR) {
        HAPLogError(&logObject, "listen() failed: %d", WSAGetLastError());
        closesocket(listener);
        return kHAPError_Unknown;
    }

    runLoop.selfPipeSocket1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (runLoop.selfPipeSocket1 == INVALID_SOCKET) {
        HAPLogError(&logObject, "socket() for client failed: %d", WSAGetLastError());
        closesocket(listener);
        return kHAPError_Unknown;
    }

    if (connect(runLoop.selfPipeSocket1, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        HAPLogError(&logObject, "connect() failed: %d", WSAGetLastError());
        closesocket(listener);
        closesocket(runLoop.selfPipeSocket1);
        runLoop.selfPipeSocket1 = INVALID_SOCKET;
        return kHAPError_Unknown;
    }

    runLoop.selfPipeSocket0 = accept(listener, NULL, NULL);
    if (runLoop.selfPipeSocket0 == INVALID_SOCKET) {
        HAPLogError(&logObject, "accept() failed: %d", WSAGetLastError());
        closesocket(listener);
        closesocket(runLoop.selfPipeSocket1);
        runLoop.selfPipeSocket1 = INVALID_SOCKET;
        return kHAPError_Unknown;
    }

    closesocket(listener);

    // Set non-blocking mode
    u_long mode = 1;
    ioctlsocket(runLoop.selfPipeSocket0, FIONBIO, &mode);
    ioctlsocket(runLoop.selfPipeSocket1, FIONBIO, &mode);

    // Register self-pipe socket
    HAPError err = HAPPlatformFileHandleRegister(
            &runLoop.selfPipeFileHandle,
            (int)runLoop.selfPipeSocket0,
            (HAPPlatformFileHandleEvent) { .isReadyForReading = true },
            SelfPipeHandleCallback,
            NULL);
    if (err) {
        HAPAssert(err == kHAPError_OutOfResources);
        ClosePipe(runLoop.selfPipeSocket0, runLoop.selfPipeSocket1);
        runLoop.selfPipeSocket0 = INVALID_SOCKET;
        runLoop.selfPipeSocket1 = INVALID_SOCKET;
        return err;
    }

    return kHAPError_None;
}

void HAPPlatformRunLoopCreate(const HAPPlatformRunLoopOptions* _Nullable options HAP_UNUSED) {
    HAPPrecondition(runLoop.state == kHAPPlatformRunLoopState_Idle);

    // Initialize Winsock
    if (!runLoop.winsockInitialized) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            HAPLogError(&logObject, "WSAStartup failed: %d", result);
            HAPFatalError();
        }
        runLoop.winsockInitialized = true;
    }

    // Create wake event
    runLoop.wakeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!runLoop.wakeEvent) {
        HAPLogError(&logObject, "CreateEvent failed: %lu", GetLastError());
        HAPFatalError();
    }

    // Create self-pipe
    HAPError err = CreateSelfPipe();
    if (err) {
        HAPAssert(err == kHAPError_Unknown || err == kHAPError_OutOfResources);
        HAPFatalError();
    }

    HAPLogInfo(&logObject, "Initialized.");
}

void HAPPlatformRunLoopRelease(void) {
    if (runLoop.selfPipeFileHandle) {
        HAPPlatformFileHandleDeregister(runLoop.selfPipeFileHandle);
        runLoop.selfPipeFileHandle = 0;
    }

    ClosePipe(runLoop.selfPipeSocket0, runLoop.selfPipeSocket1);
    runLoop.selfPipeSocket0 = INVALID_SOCKET;
    runLoop.selfPipeSocket1 = INVALID_SOCKET;

    if (runLoop.wakeEvent) {
        CloseHandle(runLoop.wakeEvent);
        runLoop.wakeEvent = NULL;
    }

    if (runLoop.winsockInitialized) {
        WSACleanup();
        runLoop.winsockInitialized = false;
    }

    HAPLogInfo(&logObject, "Released.");
}

void HAPPlatformRunLoopRun(void) {
    HAPPrecondition(runLoop.state == kHAPPlatformRunLoopState_Idle);
    runLoop.state = kHAPPlatformRunLoopState_Running;

    HAPLogInfo(&logObject, "Run loop started.");

    while (runLoop.state == kHAPPlatformRunLoopState_Running) {
        // Process expired timers
        ProcessExpiredTimers();

        // Calculate timeout
        int timeout = -1;  // Infinite
        if (runLoop.timers) {
            HAPTime now = HAPPlatformClockGetCurrent();
            if (runLoop.timers->deadline <= now) {
                timeout = 0;
            } else {
                HAPTime delta = runLoop.timers->deadline - now;
                timeout = (int)(delta < INT_MAX ? delta : INT_MAX);
            }
        }

        // Build poll array
        size_t numFileHandles = 0;
        for (HAPPlatformFileHandle* fileHandle = runLoop.fileHandles->nextFileHandle;
             fileHandle != runLoop.fileHandles;
             fileHandle = fileHandle->nextFileHandle) {
            numFileHandles++;
        }

        if (numFileHandles == 0) {
            Sleep(timeout >= 0 ? timeout : 100);
            continue;
        }

        WSAPOLLFD* pollFds = calloc(numFileHandles, sizeof(WSAPOLLFD));
        if (!pollFds) {
            HAPLogError(&logObject, "Cannot allocate poll array.");
            continue;
        }

        size_t pollIdx = 0;
        for (HAPPlatformFileHandle* fileHandle = runLoop.fileHandles->nextFileHandle;
             fileHandle != runLoop.fileHandles;
             fileHandle = fileHandle->nextFileHandle) {
            pollFds[pollIdx].fd = fileHandle->fileDescriptor;
            pollFds[pollIdx].events = 0;
            if (fileHandle->interests.isReadyForReading) pollFds[pollIdx].events |= POLLIN;
            if (fileHandle->interests.isReadyForWriting) pollFds[pollIdx].events |= POLLOUT;
            pollFds[pollIdx].revents = 0;
            fileHandle->isAwaitingEvents = true;
            pollIdx++;
        }

        // Poll
        int numReady = WSAPoll(pollFds, (ULONG)numFileHandles, timeout);

        if (numReady == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error != WSAEINTR) {
                HAPLogError(&logObject, "WSAPoll failed: %d", error);
            }
            free(pollFds);
            continue;
        }

        // Process ready file handles
        if (numReady > 0) {
            ProcessSelectedFileHandles(pollFds, (int)numFileHandles);
        }

        free(pollFds);
    }

    runLoop.state = kHAPPlatformRunLoopState_Idle;
    HAPLogInfo(&logObject, "Run loop stopped.");
}

void HAPPlatformRunLoopStop(void) {
    if (runLoop.state == kHAPPlatformRunLoopState_Running) {
        runLoop.state = kHAPPlatformRunLoopState_Stopping;
        SetEvent(runLoop.wakeEvent);

        // Wake up poll by sending to self-pipe
        char byte = 0;
        send(runLoop.selfPipeSocket1, &byte, 1, 0);
    }
}

HAPError HAPPlatformRunLoopScheduleCallback(
        HAPPlatformRunLoopCallback callback,
        void* _Nullable context,
        size_t contextSize) {
    HAPPrecondition(callback);
    HAPPrecondition(!contextSize || context);
    HAPPrecondition(contextSize <= UINT8_MAX);

    if (runLoop.numSelfPipeBytes > 0) {
        HAPLog(&logObject, "Self-pipe buffer already in use.");
        return kHAPError_OutOfResources;
    }

    HAPRawBufferCopyBytes(&runLoop.selfPipeBytes[0], &callback, sizeof callback);
    runLoop.selfPipeBytes[sizeof callback] = (uint8_t)contextSize;
    if (contextSize) {
        HAPRawBufferCopyBytes(&runLoop.selfPipeBytes[sizeof callback + 1], context, contextSize);
    }
    runLoop.numSelfPipeBytes = sizeof callback + 1 + contextSize;

    // Wake up the run loop
    char byte = 0;
    send(runLoop.selfPipeSocket1, &byte, 1, 0);

    return kHAPError_None;
}
