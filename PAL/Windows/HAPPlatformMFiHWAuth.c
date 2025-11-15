// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include "HAPPlatform.h"
#include "HAPPlatformMFiHWAuth+Init.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "MFiHWAuth" };

// Windows stub implementation - MFi hardware authentication requires I2C hardware
// Users can extend this with Windows I2C drivers if needed
// For most use cases, Software Token authentication is recommended

void HAPPlatformMFiHWAuthCreate(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    HAPLogDebug(&logObject, "%s", __func__);
    HAPLogInfo(&logObject, "MFi HW Auth not supported on Windows platform - use Software Token instead.");

    HAPRawBufferZero(mfiHWAuth, sizeof *mfiHWAuth);
    mfiHWAuth->enabled = false;
}

void HAPPlatformMFiHWAuthRelease(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    HAPLogDebug(&logObject, "%s", __func__);
    // Nothing to release
}

HAP_RESULT_USE_CHECK
bool HAPPlatformMFiHWAuthIsPoweredOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    return false; // Always disabled on Windows
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthPowerOn(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);

    HAPLogError(&logObject, "MFi HW Auth power on failed: Not supported on Windows.");
    return kHAPError_Unknown;
}

void HAPPlatformMFiHWAuthPowerOff(HAPPlatformMFiHWAuthRef mfiHWAuth) {
    HAPPrecondition(mfiHWAuth);
    // Nothing to do
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthWrite(HAPPlatformMFiHWAuthRef mfiHWAuth, const void* bytes, size_t numBytes) {
    HAPPrecondition(mfiHWAuth);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes);

    HAPLogError(&logObject, "MFi HW Auth write failed: Not supported on Windows.");
    return kHAPError_Unknown;
}

HAP_RESULT_USE_CHECK
HAPError HAPPlatformMFiHWAuthRead(
        HAPPlatformMFiHWAuthRef mfiHWAuth,
        uint8_t registerAddress,
        void* bytes,
        size_t numBytes) {
    HAPPrecondition(mfiHWAuth);
    HAPPrecondition(bytes);
    HAPPrecondition(numBytes >= 1 && numBytes <= 128);

    HAPLogError(&logObject, "MFi HW Auth read failed: Not supported on Windows.");
    return kHAPError_Unknown;
}
