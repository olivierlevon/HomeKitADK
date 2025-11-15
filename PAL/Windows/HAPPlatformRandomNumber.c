// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <Windows.h>
#include <bcrypt.h>

#include "HAPPlatform.h"

/**
 * Windows Random Number generator.
 *
 * This Random Number generator makes use of the Windows BCryptGenRandom API.
 * This is the recommended cryptographically secure random number generator for Windows.
 *
 * For more information see:
 *  - BCryptGenRandom: https://docs.microsoft.com/en-us/windows/win32/api/bcrypt/nf-bcrypt-bcryptgenrandom
 */

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "RandomNumber" };

void HAPPlatformRandomNumberFill(void* bytes, size_t numBytes) {
    HAPPrecondition(bytes);

    // Read random data with retry logic
    for (int i = 0; i < 5; i++) {
        NTSTATUS status = BCryptGenRandom(
                NULL, // Use default system RNG
                (PUCHAR) bytes,
                (ULONG) numBytes,
                BCRYPT_USE_SYSTEM_PREFERRED_RNG);

        if (!BCRYPT_SUCCESS(status)) {
            HAPLogError(&logObject, "BCryptGenRandom failed with status: 0x%08lx.", status);
            if (i == 4) { // Last retry
                HAPFatalError();
            }
            continue;
        }

        // Verify random data is not all zeros
        if (numBytes < 128 / 8 || !HAPRawBufferIsZero(bytes, numBytes)) {
            return;
        }
    }

    HAPLogError(&logObject, "BCryptGenRandom produced only zeros.");
    HAPFatalError();
}
