// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <Windows.h>

#include "HAPPlatform.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "Clock" };

HAPTime HAPPlatformClockGetCurrent(void) {
    static bool isInitialized = false;
    static HAPTime previousNow = 0;
    static LARGE_INTEGER frequency;
    static LARGE_INTEGER startCounter;

    // Initialize on first call
    if (!isInitialized) {
        if (!QueryPerformanceFrequency(&frequency)) {
            HAPLogError(&logObject, "QueryPerformanceFrequency failed.");
            HAPFatalError();
        }
        if (!QueryPerformanceCounter(&startCounter)) {
            HAPLogError(&logObject, "QueryPerformanceCounter failed.");
            HAPFatalError();
        }
        HAPLog(&logObject, "Using QueryPerformanceCounter for monotonic time.");
        isInitialized = true;
    }

    // Get current counter
    LARGE_INTEGER currentCounter;
    if (!QueryPerformanceCounter(&currentCounter)) {
        HAPLogError(&logObject, "QueryPerformanceCounter failed.");
        HAPFatalError();
    }

    // Calculate elapsed time in milliseconds
    LONGLONG elapsed = currentCounter.QuadPart - startCounter.QuadPart;
    HAPTime now = (HAPTime)((elapsed * 1000) / frequency.QuadPart);

    // Check for time going backwards (should not happen with QueryPerformanceCounter)
    if (now < previousNow) {
        HAPLog(&logObject, "Time jumped backwards by %llu ms.", (unsigned long long)(previousNow - now));
        HAPFatalError();
    }

    // Check for overflow
    if (now & (1ull << 63)) {
        HAPLog(&logObject, "Time overflowed (capped at 2^63 - 1).");
        HAPFatalError();
    }

    previousNow = now;
    return now;
}
