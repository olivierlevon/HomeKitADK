// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include "HAPPlatform.h"

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

static char* osVersion = NULL;

static const char* GetOSVersion(void) {
    if (!osVersion) {
        OSVERSIONINFOEX osvi;
        ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

#pragma warning(push)
#pragma warning(disable: 4996) // GetVersionEx is deprecated but still useful
        if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
            osVersion = (char*)malloc(64);
            sprintf(osVersion, "%lu.%lu.%lu", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        } else {
            osVersion = "Unknown";
        }
#pragma warning(pop)
    }
    return osVersion;
}

HAP_RESULT_USE_CHECK
uint32_t HAPPlatformGetCompatibilityVersion(void) {
    return HAP_PLATFORM_COMPATIBILITY_VERSION;
}

HAP_RESULT_USE_CHECK
const char* HAPPlatformGetIdentification(void) {
    return "Windows";
}

HAP_RESULT_USE_CHECK
const char* HAPPlatformGetVersion(void) {
    return GetOSVersion();
}

HAP_RESULT_USE_CHECK
const char* HAPPlatformGetBuild(void) {
    HAP_DIAGNOSTIC_PUSH
    HAP_DIAGNOSTIC_IGNORED_CLANG("-Wdate-time")
    const char* build = __DATE__ " " __TIME__;
    HAP_DIAGNOSTIC_POP
    return build;
}
