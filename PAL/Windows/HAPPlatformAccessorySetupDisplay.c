// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <stdio.h>

#include "HAPPlatform.h"
#include "HAPPlatformAccessorySetupDisplay+Init.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "AccessorySetupDisplay" };

static void DisplaySetupInfo(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    HAPPrecondition(setupDisplay);
    HAPPrecondition(setupDisplay->setupPayloadIsSet);
    HAPPrecondition(setupDisplay->setupCodeIsSet);

    HAPLogInfo(
            &logObject,
            "##### Setup code for display: %s",
            setupDisplay->setupCode.stringValue);
    HAPLogInfo(
            &logObject,
            "##### Setup payload for QR code: %s",
            setupDisplay->setupPayload.stringValue);

    // On Windows, we just print to console
    // Users can install qrencode manually or use online QR code generators
    printf("\n");
    printf("===================================\n");
    printf("  HomeKit Setup Information\n");
    printf("===================================\n");
    printf("Setup Code: %s\n", setupDisplay->setupCode.stringValue);
    printf("Setup Payload: %s\n", setupDisplay->setupPayload.stringValue);
    printf("===================================\n");
    printf("\n");
}

void HAPPlatformAccessorySetupDisplayCreate(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    HAPPrecondition(HAVE_DISPLAY);
    HAPPrecondition(setupDisplay);

    HAPRawBufferZero(setupDisplay, sizeof *setupDisplay);
}

void HAPPlatformAccessorySetupDisplayUpdateSetupPayload(
        HAPPlatformAccessorySetupDisplayRef setupDisplay,
        const HAPSetupPayload* _Nullable setupPayload,
        const HAPSetupCode* _Nullable setupCode) {
    HAPPrecondition(HAVE_DISPLAY);
    HAPPrecondition(setupDisplay);

    if (setupCode) {
        HAPLogInfo(&logObject, "##### Setup code for display: %s", setupCode->stringValue);
        HAPRawBufferCopyBytes(&setupDisplay->setupCode, HAPNonnull(setupCode), sizeof setupDisplay->setupCode);
        setupDisplay->setupCodeIsSet = true;
    } else {
        HAPLogInfo(&logObject, "##### Setup code for display invalidated.");
        HAPRawBufferZero(&setupDisplay->setupCode, sizeof setupDisplay->setupCode);
        setupDisplay->setupCodeIsSet = false;
    }
    if (setupPayload) {
        HAPLogInfo(&logObject, "##### Setup payload for QR code display: %s", setupPayload->stringValue);
        HAPRawBufferCopyBytes(&setupDisplay->setupPayload, HAPNonnull(setupPayload), sizeof setupDisplay->setupPayload);
        setupDisplay->setupPayloadIsSet = true;
    } else {
        HAPRawBufferZero(&setupDisplay->setupPayload, sizeof setupDisplay->setupPayload);
        setupDisplay->setupPayloadIsSet = false;
    }

    if (setupDisplay->setupPayloadIsSet) {
        DisplaySetupInfo(setupDisplay);
    }
}

void HAPPlatformAccessorySetupDisplayHandleStartPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    HAPPrecondition(HAVE_DISPLAY);
    HAPPrecondition(setupDisplay);
    HAPPrecondition(setupDisplay->setupCodeIsSet);

    HAPLogInfo(
            &logObject, "##### Pairing attempt has started with setup code: %s.", setupDisplay->setupCode.stringValue);

    if (setupDisplay->setupPayloadIsSet) {
        DisplaySetupInfo(setupDisplay);
    }
}

void HAPPlatformAccessorySetupDisplayHandleStopPairing(HAPPlatformAccessorySetupDisplayRef setupDisplay) {
    HAPPrecondition(HAVE_DISPLAY);
    HAPPrecondition(setupDisplay);

    HAPLogInfo(&logObject, "##### Pairing attempt has completed or has been canceled.");
}
