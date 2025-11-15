// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#include <Windows.h>
#include <process.h>

#include "HAPPlatform.h"
#include "HAPPlatformAccessorySetupNFC+Init.h"

#if HAVE_NFC
#include <nfc/nfc.h>
#include <nfc/nfc-emulation.h>
#endif

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "AccessorySetupNFC" };

#if HAVE_NFC

HAP_ENUM_BEGIN(uint8_t, NFCCommand) {
    /**
     * READ.
     */
    kNFCCommand_Read = 0x30,

    /**
     * HALT.
     */
    kNFCCommand_Halt = 0x50,
} HAP_ENUM_END(uint8_t, NFCCommand);

typedef struct {
    /**
     * NFC payload.
     */
    const void* payloadBytes;

    /**
     * Length of NFC payload.
     */
    size_t numPayloadBytes;
} NFCUserData;

static int NFCIOCallback(
        struct nfc_emulator* emulator,
        const uint8_t* data_in,
        const size_t data_in_len,
        uint8_t* data_out,
        const size_t data_out_len) {
    HAPPrecondition(emulator->user_data);
    const NFCUserData* userData = emulator->user_data;
    const uint8_t* payloadBytes = userData->payloadBytes;
    size_t numPayloadBytes = userData->numPayloadBytes;

    HAPLogBufferDebug(&logObject, data_in, data_in_len, "NFC In.");

    if (data_in_len < 1) {
        HAPLogError(&logObject, "NFC IO callback invoked without input data.");
        return -EINVAL;
    }

    uint8_t command = data_in[0];
    switch (command) {
        case kNFCCommand_Read: {
            if (data_out_len < 16) {
                HAPLogError(&logObject, "NFC IO callback: READ invoked with too small output buffer.");
                return -ENOSPC;
            }
            uint8_t offset = data_in[1];
            uint8_t numBytes = 16;
            if (offset > (numPayloadBytes - numBytes) / 4) {
                HAPLogError(&logObject, "NFC IO callback: READ invoked with out-of-range offset %u.", offset);
                return -EINVAL;
            }
            HAPAssert(numPayloadBytes >= numBytes);
            HAPRawBufferCopyBytes(data_out, &payloadBytes[offset * 4], numBytes);
            HAPLogBufferDebug(&logObject, data_out, numBytes, "NFC Out.");
            return numBytes;
        } break;
        case kNFCCommand_Halt: {
            HAPLogDebug(&logObject, "NFC IO callback: HALT sent.");
            return -ECONNABORTED;
        } break;
        default: {
            HAPLog(&logObject, "NFC IO callback: Unsupported command (0x%02x).", command);
            return -ENOTSUP;
        } break;
    }
    HAPFatalError();
}

/**
 * Entry point for the thread that manages NFC.
 */
static unsigned int __stdcall NFCMain(void* context) {
    HAPPrecondition(context);
    HAPPlatformAccessorySetupNFCRef setupNFC = context;
    HAPAssert(setupNFC->nfc.isActive);

    while (InterlockedCompareExchange(&setupNFC->nfc.nfcLock, 1, 0) != 0) {
        Sleep(1);
    }
    HAPLogDebug(&logObject, "Started NFC thread...");

    // Initialize NFC library
    HAPAssert(!setupNFC->nfc.nfcContext);
    nfc_init(&setupNFC->nfc.nfcContext);
    if (!setupNFC->nfc.nfcContext) {
        HAPLogError(&logObject, "Unable to init libnfc (malloc). Continuing without NFC.");
        goto cleanup;
    }
    const char* _Nullable connectionString = setupNFC->libnfcConnectionString;
    HAPLog(&logObject, "libnfc version: %s.", nfc_version());
    HAPLog(&logObject, "libnfc connection string: %s.", connectionString ? connectionString : "(default)");

    // Connect to NFC hardware
    do {
        HAPLogDebug(&logObject, "Opening NFC device.");
        HAPAssert(!setupNFC->nfc.nfcDevice);
        setupNFC->nfc.nfcDevice = nfc_open(setupNFC->nfc.nfcContext, connectionString);
        if (!setupNFC->nfc.nfcDevice) {
            HAPLogInfo(&logObject, "Unable to open NFC device. Retrying...");
            InterlockedExchange(&setupNFC->nfc.nfcLock, 0);
            Sleep(500);
            while (InterlockedCompareExchange(&setupNFC->nfc.nfcLock, 1, 0) != 0) {
                Sleep(1);
            }
            if (setupNFC->nfc.isAborted) {
                goto cleanup;
            }
        }
    } while (!setupNFC->nfc.nfcDevice);
    HAPLogDebug(&logObject, "NFC device: %s opened.", nfc_device_get_name(setupNFC->nfc.nfcDevice));

    // Prepare NDEF payload
    // Based on http://www.libnfc.org/api/nfc-emulate-forum-tag2_8c_source.html
    HAPAssert(HAPStringGetNumBytes(setupNFC->nfc.setupPayload.stringValue) == 20);
    const uint8_t* c = (const uint8_t*) setupNFC->nfc.setupPayload.stringValue;
    uint8_t nfcPayload[] = {
        // READ requests always return 16 bytes.
        // To reflect this, 16-byte chunks are grouped together and separated by an empty line below.

        // Block 0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // Block 2 (Static lock bytes: CC area and data area are read-only locked)
        0x00, 0x00, 0xFF, 0xFF,
        // Block 3 (CC - NFC-Forum Tag Type 2 version 1.0)
        0xE1, 0x10, 0x04, 0x0F,

        // Block 4 (NDEF)
        0x03, 5 + 20, 0xd1, 0x01, 0x15, 0x55, 0x00,
        c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7], c[8],

        c[9], c[10], c[11], c[12], c[13], c[14], c[15], c[16], c[17], c[18], c[19],
        0x00, 0x00, 0x00, 0x00, 0x00
    };
    NFCUserData nfcUserData = { .payloadBytes = nfcPayload, .numPayloadBytes = sizeof nfcPayload };

    // Emulate NDEF tag
    nfc_target nfcTarget = {
        .nm = {
            .nmt = NMT_ISO14443A,
            .nbr = NBR_UNDEFINED,
        },
        .nti.nai = {
            .abtAtqa = { 0x00, 0x04 },
            .btSak = 0x00,
            .szUidLen = 4,
            .abtUid = { 0x08, 0x00, 0xb0, 0x0b },
            .szAtsLen = 0,
            .abtAts = { 0x00 }
        }
    };
    struct nfc_emulation_state_machine nfcStateMachine = { .io = NFCIOCallback, .data = nfcPayload };
    struct nfc_emulator nfcEmulator = {
        .target = &nfcTarget,
        .state_machine = &nfcStateMachine,
        .user_data = &nfcUserData,
    };
    HAPLogInfo(&logObject, "NFC enabled.");
    while (!setupNFC->nfc.isAborted) {
        setupNFC->nfc.isEmulating = true;
        InterlockedExchange(&setupNFC->nfc.nfcLock, 0);
        HAPLogDebug(&logObject, "Starting NFC card emulation.");
        int e = nfc_emulate_target(setupNFC->nfc.nfcDevice, &nfcEmulator, /* timeout: */ 0);
        while (InterlockedCompareExchange(&setupNFC->nfc.nfcLock, 1, 0) != 0) {
            Sleep(1);
        }
        setupNFC->nfc.isEmulating = false;
        if (e) {
            if (e == -ECONNABORTED) {
                HAPLogDebug(&logObject, "NFC transaction complete.");
            } else {
                HAPLogInfo(&logObject, "`nfc_emulate_target` failed: %d.", e);
            }
        }
    }
    HAPLogInfo(&logObject, "NFC disabled.");

cleanup:
    HAPLogDebug(&logObject, "Releasing NFC resources.");

    if (setupNFC->nfc.nfcDevice) {
        nfc_close(setupNFC->nfc.nfcDevice);
        setupNFC->nfc.nfcDevice = NULL;
    }

    if (setupNFC->nfc.nfcContext) {
        nfc_exit(setupNFC->nfc.nfcContext);
        setupNFC->nfc.nfcContext = NULL;
    }

    InterlockedExchange(&setupNFC->nfc.nfcLock, 0);
    return 0;
}

#endif

/**
 * Stops programmable NFC advertisement if active.
 *
 * @param      setupNFC             Accessory setup programmable NFC tag.
 */
static void NFCStop(HAPPlatformAccessorySetupNFCRef setupNFC) {
    HAPPrecondition(setupNFC);

#if HAVE_NFC
    // Stop NFC
    if (setupNFC->nfc.isActive) {
        HAPLogDebug(&logObject, "Signalling NFC thread to stop.");
        while (InterlockedCompareExchange(&setupNFC->nfc.nfcLock, 1, 0) != 0) {
            Sleep(1);
        }
        setupNFC->nfc.isAborted = true;
        while (setupNFC->nfc.isEmulating) {
            HAPAssert(setupNFC->nfc.nfcDevice);
            HAPLogDebug(&logObject, "Aborting NFC card emulation.");
            nfc_abort_command(setupNFC->nfc.nfcDevice);
            InterlockedExchange(&setupNFC->nfc.nfcLock, 0);
            Sleep(100);
            while (InterlockedCompareExchange(&setupNFC->nfc.nfcLock, 1, 0) != 0) {
                Sleep(1);
            }
        }
        InterlockedExchange(&setupNFC->nfc.nfcLock, 0);

        HAPLogDebug(&logObject, "Waiting for NFC thread to stop...");
        WaitForSingleObject(setupNFC->nfc.thread, INFINITE);
        CloseHandle(setupNFC->nfc.thread);
        HAPLogDebug(&logObject, "Stopped NFC thread.");
        HAPRawBufferZero(&setupNFC->nfc, sizeof setupNFC->nfc);
    }
    HAPAssert(!setupNFC->nfc.isActive);
#endif
}

/**
 * Sets the NFC NDEF payload.
 *
 * @param      setupNFC             Accessory setup programmable NFC tag.
 * @param      payloadString        NFC payload string.
 */
static void NFCStart(HAPPlatformAccessorySetupNFCRef setupNFC, char const* payloadString) {
    HAPPrecondition(setupNFC);
    HAPPrecondition(payloadString);

#if HAVE_NFC
    // Stop NFC if active
    if (setupNFC->nfc.isActive) {
        NFCStop(setupNFC);
    }

    // Copy arguments
    size_t numPayloadStringBytes = HAPStringGetNumBytes(payloadString);
    HAPAssert(numPayloadStringBytes < sizeof setupNFC->nfc.setupPayload.stringValue);
    HAPRawBufferCopyBytes(setupNFC->nfc.setupPayload.stringValue, payloadString, numPayloadStringBytes);

    // Start new NFC thread
    HAPLogDebug(&logObject, "Starting NFC thread.");
    setupNFC->nfc.isActive = true;
    setupNFC->nfc.thread = (HANDLE)_beginthreadex(
            NULL,           // security
            0,              // stack size
            NFCMain,        // start address
            setupNFC,       // arg list
            0,              // init flag
            NULL);          // thread identifier

    if (setupNFC->nfc.thread == 0) {
        HAPLogError(&logObject, "_beginthreadex failed to create NFC thread: Continuing without NFC.");
        HAPRawBufferZero(&setupNFC->nfc, sizeof setupNFC->nfc);
        return;
    }
#endif
}

void HAPPlatformAccessorySetupNFCCreate(
        HAPPlatformAccessorySetupNFCRef setupNFC,
        const HAPPlatformAccessorySetupNFCOptions* options) {
    HAPPrecondition(HAVE_NFC);
    HAPPrecondition(setupNFC);
    HAPPrecondition(options);

    HAPRawBufferZero(setupNFC, sizeof *setupNFC);

    setupNFC->libnfcConnectionString = options->libnfcConnectionString;
}

void HAPPlatformAccessorySetupNFCRelease(HAPPlatformAccessorySetupNFCRef setupNFC) {
    HAPPrecondition(HAVE_NFC);
    HAPPrecondition(setupNFC);

    NFCStop(setupNFC);
}

void HAPPlatformAccessorySetupNFCUpdateSetupPayload(
        HAPPlatformAccessorySetupNFCRef setupNFC,
        const HAPSetupPayload* setupPayload,
        bool isPairable) {
    HAPPrecondition(HAVE_NFC);
    HAPPrecondition(setupNFC);
    HAPPrecondition(setupPayload);

    HAPLogInfo(
            &logObject,
            "##### Setup payload for programmable NFC: %s (%s)",
            setupPayload->stringValue,
            isPairable ? "pairable" : "not pairable");
    NFCStart(setupNFC, setupPayload->stringValue);
}
