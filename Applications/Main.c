// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

#ifdef _WIN32
#ifdef _DEBUG 
#define _CRTDBG_MAP_ALLOC 
#endif

#ifdef _DEBUG 
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Iphlpapi.h>
#include <windows.h>
#endif

#include "App.h"
#include "DB.h"

#include "HAP.h"
#include "HAPPlatform+Init.h"
#include "HAPPlatformAccessorySetup+Init.h"
#include "HAPPlatformBLEPeripheralManager+Init.h"
#include "HAPPlatformKeyValueStore+Init.h"
#include "HAPPlatformMFiHWAuth+Init.h"
#include "HAPPlatformMFiTokenAuth+Init.h"
#include "HAPPlatformRunLoop+Init.h"
#if IP
#include "HAPPlatformServiceDiscovery+Init.h"
#include "HAPPlatformTCPStreamManager+Init.h"
#endif
#if HAVE_NFC
#include "HAPPlatformAccessorySetupNFC+Init.h"
#endif

#ifdef _WIN32
#if defined(_DEBUG)
#ifdef VLD
#include <vld.h> 
#endif

//#if defined(_DEBUG)
#ifdef WSOCK_TRACE
#pragma comment(lib, "wsock_trace.lib")  //"C:\Users\Olivier\Desktop\wsock_trace\src\wsock_trace.lib"
#else
 #pragma comment(lib, "Ws2_32.lib")
#endif
#else
 #pragma comment(lib, "Ws2_32.lib")
#endif



#else
#include <signal.h>
#endif

static bool requestedFactoryReset = false;
static bool clearPairings = false;

//HANDLE gStopEvent = NULL;

#define PREFERRED_ADVERTISING_INTERVAL (HAPBLEAdvertisingIntervalCreateFromMilliseconds(417.5f))

/**
 * Global platform objects.
 * Only tracks objects that will be released in DeinitializePlatform.
 */
static struct {
    HAPPlatformKeyValueStore keyValueStore;
    HAPAccessoryServerOptions hapAccessoryServerOptions;
    HAPPlatform hapPlatform;
    HAPAccessoryServerCallbacks hapAccessoryServerCallbacks;

#if HAVE_NFC
    HAPPlatformAccessorySetupNFC setupNFC;
#endif

#if IP
    HAPPlatformTCPStreamManager tcpStreamManager;
#endif

    HAPPlatformMFiHWAuth mfiHWAuth;
    HAPPlatformMFiTokenAuth mfiTokenAuth;
} platform;

/**
 * HomeKit accessory server that hosts the accessory.
 */
static HAPAccessoryServerRef accessoryServer;

void HandleUpdatedState(HAPAccessoryServerRef* _Nonnull server, void* _Nullable context);

/**
 * Functions provided by App.c for each accessory application.
 */
extern void AppRelease(void);
extern void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore);
extern void AppInitialize(
        HAPAccessoryServerOptions* hapAccessoryServerOptions HAP_UNUSED,
        HAPPlatform* hapPlatform HAP_UNUSED,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks HAP_UNUSED);
extern void AppDeinitialize(void);
extern void AppAccessoryServerStart(void);
extern void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context);
extern const HAPAccessory* AppGetAccessoryInfo(void);

#ifdef _WIN32
#ifndef HeapEnableTerminationOnCorruption
#	define HeapEnableTerminationOnCorruption (HEAP_INFORMATION_CLASS)1
#endif

//static 
//extern BOOL WINAPI ConsoleControlHandler(DWORD inControlEvent);
#endif

/**
 * Initialize global platform objects.
 */
static void InitializePlatform(void) {
    // Key-value store.
    HAPPlatformKeyValueStoreCreate(
            &platform.keyValueStore, &(const HAPPlatformKeyValueStoreOptions) { .rootDirectory = ".HomeKitStore" }); //"C:\\Users\\Olivier\\Desktop\\HomeKitADK\\Build\\Debug\\.HomeKitStore"
    platform.hapPlatform.keyValueStore = &platform.keyValueStore;

    // Accessory setup manager. Depends on key-value store.
    static HAPPlatformAccessorySetup accessorySetup;
    HAPPlatformAccessorySetupCreate(
            &accessorySetup, &(const HAPPlatformAccessorySetupOptions) { .keyValueStore = &platform.keyValueStore });
    platform.hapPlatform.accessorySetup = &accessorySetup;

#if IP
    // TCP stream manager.
    HAPPlatformTCPStreamManagerCreate(
            &platform.tcpStreamManager,
            &(const HAPPlatformTCPStreamManagerOptions) {
                    .interfaceName = NULL,       // Listen on all available network interfaces.
                    .port = kHAPNetworkPort_Any, // Listen on unused port number from the ephemeral port range.
                    .maxConcurrentTCPStreams = kHAPIPSessionStorage_DefaultNumElements });

    // Service discovery.
    static HAPPlatformServiceDiscovery serviceDiscovery;
    HAPPlatformServiceDiscoveryCreate(
            &serviceDiscovery,
            &(const HAPPlatformServiceDiscoveryOptions) {
                    0 /* Register services on all available network interfaces. */
            });
    platform.hapPlatform.ip.serviceDiscovery = &serviceDiscovery;
#endif

#if (BLE)
    // BLE peripheral manager. Depends on key-value store.
    static HAPPlatformBLEPeripheralManagerOptions blePMOptions = { 0 };
    blePMOptions.keyValueStore = &platform.keyValueStore;

    static HAPPlatformBLEPeripheralManager blePeripheralManager;
    HAPPlatformBLEPeripheralManagerCreate(&blePeripheralManager, &blePMOptions);
    platform.hapPlatform.ble.blePeripheralManager = &blePeripheralManager;
#endif

#if HAVE_MFI_HW_AUTH
    // Apple Authentication Coprocessor provider.
    HAPPlatformMFiHWAuthCreate(&platform.mfiHWAuth);
#endif

#if HAVE_MFI_HW_AUTH
    platform.hapPlatform.authentication.mfiHWAuth = &platform.mfiHWAuth;
#endif

    // Software Token provider. Depends on key-value store.
    HAPPlatformMFiTokenAuthCreate(
            &platform.mfiTokenAuth,
            &(const HAPPlatformMFiTokenAuthOptions) { .keyValueStore = &platform.keyValueStore });

    // Run loop.
    HAPPlatformRunLoopCreate(&(const HAPPlatformRunLoopOptions) { .keyValueStore = &platform.keyValueStore });

    platform.hapAccessoryServerOptions.maxPairings = kHAPPairingStorage_MinElements;

    platform.hapPlatform.authentication.mfiTokenAuth =
            HAPPlatformMFiTokenAuthIsProvisioned(&platform.mfiTokenAuth) ? &platform.mfiTokenAuth : NULL;

    platform.hapAccessoryServerCallbacks.handleUpdatedState = HandleUpdatedState;
}

/**
 * Deinitialize global platform objects.
 */
static void DeinitializePlatform(void) {
#if HAVE_MFI_HW_AUTH
    // Apple Authentication Coprocessor provider.
    HAPPlatformMFiHWAuthRelease(&platform.mfiHWAuth);
#endif

#if IP
    // TCP stream manager.
    HAPPlatformTCPStreamManagerRelease(&platform.tcpStreamManager);
#endif

    AppDeinitialize();

    // Run loop.
    HAPPlatformRunLoopRelease();
}

/**
 * Restore platform specific factory settings.
 */
void RestorePlatformFactorySettings(void) {
}

/**
 * Either simply passes State handling to app, or processes Factory Reset
 */
void HandleUpdatedState(HAPAccessoryServerRef* _Nonnull server, void* _Nullable context) {
    if (HAPAccessoryServerGetState(server) == kHAPAccessoryServerState_Idle && requestedFactoryReset) {
        HAPPrecondition(server);

        HAPError err;

        HAPLogInfo(&kHAPLog_Default, "A factory reset has been requested.");

        // Purge app state.
        err = HAPPlatformKeyValueStorePurgeDomain(&platform.keyValueStore, ((HAPPlatformKeyValueStoreDomain) 0x00));
        if (err) {
            HAPAssert(err == kHAPError_Unknown);
            HAPFatalError();
        }

        // Reset HomeKit state.
        err = HAPRestoreFactorySettings(&platform.keyValueStore);
        if (err) {
            HAPAssert(err == kHAPError_Unknown);
            HAPFatalError();
        }

        // Restore platform specific factory settings.
        RestorePlatformFactorySettings();

        // De-initialize App.
        AppRelease();

        requestedFactoryReset = false;

        // Re-initialize App.
        AppCreate(server, &platform.keyValueStore);

        // Restart accessory server.
        AppAccessoryServerStart();
        return;
    } else if (HAPAccessoryServerGetState(server) == kHAPAccessoryServerState_Idle && clearPairings) {
        HAPError err;
        err = HAPRemoveAllPairings(&platform.keyValueStore);
        if (err) {
            HAPAssert(err == kHAPError_Unknown);
            HAPFatalError();
        }
        AppAccessoryServerStart();
    } else {
        AccessoryServerHandleUpdatedState(server, context);
    }
}

#if IP
static void InitializeIP(void) {
    // Prepare accessory server storage.
    static HAPIPSession ipSessions[kHAPIPSessionStorage_DefaultNumElements];
    static uint8_t ipInboundBuffers[HAPArrayCount(ipSessions)][kHAPIPSession_DefaultInboundBufferSize];
    static uint8_t ipOutboundBuffers[HAPArrayCount(ipSessions)][kHAPIPSession_DefaultOutboundBufferSize];
    static HAPIPEventNotificationRef ipEventNotifications[HAPArrayCount(ipSessions)][kAttributeCount];
    for (size_t i = 0; i < HAPArrayCount(ipSessions); i++) {
        ipSessions[i].inboundBuffer.bytes = ipInboundBuffers[i];
        ipSessions[i].inboundBuffer.numBytes = sizeof ipInboundBuffers[i];
        ipSessions[i].outboundBuffer.bytes = ipOutboundBuffers[i];
        ipSessions[i].outboundBuffer.numBytes = sizeof ipOutboundBuffers[i];
        ipSessions[i].eventNotifications = ipEventNotifications[i];
        ipSessions[i].numEventNotifications = HAPArrayCount(ipEventNotifications[i]);
    }
    static HAPIPReadContextRef ipReadContexts[kAttributeCount];
    static HAPIPWriteContextRef ipWriteContexts[kAttributeCount];
    static uint8_t ipScratchBuffer[kHAPIPSession_DefaultScratchBufferSize];
    static HAPIPAccessoryServerStorage ipAccessoryServerStorage = {
        .sessions = ipSessions,
        .numSessions = HAPArrayCount(ipSessions),
        .readContexts = ipReadContexts,
        .numReadContexts = HAPArrayCount(ipReadContexts),
        .writeContexts = ipWriteContexts,
        .numWriteContexts = HAPArrayCount(ipWriteContexts),
        .scratchBuffer = { .bytes = ipScratchBuffer, .numBytes = sizeof ipScratchBuffer }
    };

    platform.hapAccessoryServerOptions.ip.transport = &kHAPAccessoryServerTransport_IP;
    platform.hapAccessoryServerOptions.ip.accessoryServerStorage = &ipAccessoryServerStorage;

    platform.hapPlatform.ip.tcpStreamManager = &platform.tcpStreamManager;
}
#endif

#if BLE
static void InitializeBLE(void) {
    static HAPBLEGATTTableElementRef gattTableElements[kAttributeCount];
    static HAPBLESessionCacheElementRef sessionCacheElements[kHAPBLESessionCache_MinElements];
    static HAPSessionRef session;
    static uint8_t procedureBytes[2048];
    static HAPBLEProcedureRef procedures[1];

    static HAPBLEAccessoryServerStorage bleAccessoryServerStorage = {
        .gattTableElements = gattTableElements,
        .numGATTTableElements = HAPArrayCount(gattTableElements),
        .sessionCacheElements = sessionCacheElements,
        .numSessionCacheElements = HAPArrayCount(sessionCacheElements),
        .session = &session,
        .procedures = procedures,
        .numProcedures = HAPArrayCount(procedures),
        .procedureBuffer = { .bytes = procedureBytes, .numBytes = sizeof procedureBytes }
    };

    platform.hapAccessoryServerOptions.ble.transport = &kHAPAccessoryServerTransport_BLE;
    platform.hapAccessoryServerOptions.ble.accessoryServerStorage = &bleAccessoryServerStorage;
    platform.hapAccessoryServerOptions.ble.preferredAdvertisingInterval = PREFERRED_ADVERTISING_INTERVAL;
    platform.hapAccessoryServerOptions.ble.preferredNotificationDuration = kHAPBLENotification_MinDuration;
}
#endif

#ifdef _WIN32
//static 
BOOL WINAPI	ConsoleControlHandler( DWORD inControlEvent )
{
	BOOL			handled;
//	int		err;
	
	handled = FALSE;
	switch ( inControlEvent )
	{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
			printf( "%s Ctrl-C received inControlEvent %lu\n\n", __FUNCTION__, inControlEvent);
			printf( "\n%s Exiting................................... \n\n", __FUNCTION__);

		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
//				err = ServiceSpecificStop();
//			HAPPlatformRunLoopStop();
/*
			BOOL OK = SetEvent(gStopEvent);
			if (!OK)
			{
				DWORD d = GetLastError();
			}
*/
			handled = FALSE; //TRUE;
			break;
		
		default:
			break;
	}
	
//exit:
	return( handled );
}
#endif

int main(int argc, char* _Nullable argv[_Nullable]) {
#ifdef _WIN32
	int ret = 0;
	int tmpDbgFlag;
		
  //  ok = HeapSetInformation(GetProcessHeap(), HeapEnableTerminationOnCorruption, NULL, 0); //NULL
	//HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
//	SetHeapTerminate(NULL);


//	_CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW );
//	_CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );

    // Send all reports to STDOUT
    _CrtSetReportMode( _CRT_WARN, _CRTDBG_MODE_FILE ); //_CRTDBG_MODE_DEBUG
    _CrtSetReportFile( _CRT_WARN, _CRTDBG_FILE_STDOUT );
    _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDOUT );
    _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
    _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDOUT );

	
	// Get current flag
	tmpDbgFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

	// on by def
//	tmpDbgFlag |= _CRTDBG_ALLOC_MEM_DF;
	
	// Turn on leak-checking bit.
//	tmpDbgFlag |= _CRTDBG_LEAK_CHECK_DF;

	// Turn Off (AND) - prevent _CrtCheckMemory from  
	// being called at every allocation request  
	tmpDbgFlag &= ~_CRTDBG_CHECK_ALWAYS_DF;  

	//tmpDbgFlag |= _CRTDBG_CHECK_ALWAYS_DF;

	// Clear the upper 16 bits and OR in the desired freqency  
//	tmpDbgFlag = (tmpDbgFlag & 0x0000FFFF) | _CRTDBG_CHECK_EVERY_16_DF; 

	// Turn off CRT block checking bit.
	tmpDbgFlag &= ~_CRTDBG_CHECK_CRT_DF;

	/*  
	 * Set the debug-heap flag to keep freed blocks in the  
	 * heap's linked list - This will allow us to catch any  
	 * inadvertent use of freed memory  
	 */  
    tmpDbgFlag |= _CRTDBG_DELAY_FREE_MEM_DF;  
	
	// Set flag to the new value.
	_CrtSetDbgFlag( tmpDbgFlag );
#endif
	
    HAPAssert(HAPGetCompatibilityVersion() == HAP_COMPATIBILITY_VERSION);

#ifdef _WIN32
	{
		WSADATA wsaData;
		int res;

		res = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (res != 0)
		{
			fprintf(stderr, "Cannot initialize WinSock %d\n", res);
			ret = -1;
			goto exit;
		}
	}

	{
		bool ok;

		// Install a Console Control Handler to handle things like control-c signals.
		ok = SetConsoleCtrlHandler(ConsoleControlHandler, TRUE);
		if (!ok)
		{
			printf("SetConsoleCtrlHandler error %lu\n", GetLastError());
			ret = -1;
			goto exit;
		}
	}
#endif

    // Initialize global platform objects.
    InitializePlatform();

#if IP
    InitializeIP();
#endif

#if BLE
    InitializeBLE();
#endif

    // Perform Application-specific initializations such as setting up callbacks
    // and configure any additional unique platform dependencies
    AppInitialize(&platform.hapAccessoryServerOptions, &platform.hapPlatform, &platform.hapAccessoryServerCallbacks);

    // Initialize accessory server.
    HAPAccessoryServerCreate(
            &accessoryServer,
            &platform.hapAccessoryServerOptions,
            &platform.hapPlatform,
            &platform.hapAccessoryServerCallbacks,
            /* context: */ NULL);

    // Create app object.
    AppCreate(&accessoryServer, &platform.keyValueStore);

    // Start accessory server for App.
    AppAccessoryServerStart();

    // Run main loop until explicitly stopped.
    HAPPlatformRunLoopRun();
    // Run loop stopped explicitly by calling function HAPPlatformRunLoopStop.

    // Cleanup.
    AppRelease();

    HAPAccessoryServerRelease(&accessoryServer);

    DeinitializePlatform();

#ifdef _WIN32
	SetConsoleCtrlHandler(ConsoleControlHandler, FALSE);

	// if (gStopEvent)
		// CloseHandle(gStopEvent);
	
	// TearDownNotifications();

exit:
    WSACleanup();


#if defined(_DEBUG)
#ifndef VLD
	_CrtDumpMemoryLeaks();
#endif
#endif

	if (_isatty(_fileno(stdin)))
	{
		printf("  + Press Enter to exit this program.\n");
		fflush(stdout);
		fflush(stderr);
		(void)getchar();
	}
#endif

    return ret;
}

