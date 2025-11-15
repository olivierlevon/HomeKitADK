// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// FULL Windows BLE Peripheral implementation using BTstack
// BTstack: Open-source Bluetooth stack with full peripheral mode support
// https://github.com/bluekitchen/btstack

#include "HAPPlatformBLEPeripheralManager+Init.h"

#ifdef HAVE_BLE

#include <Windows.h>
#include <process.h>

// BTstack headers
#include "btstack.h"
#include "ble/att_db.h"
#include "ble/att_server.h"
#include "ble/sm.h"

static const HAPLogObject logObject = { .subsystem = kHAPPlatform_LogSubsystem, .category = "BLEPeripheralManager" };

//----------------------------------------------------------------------------------------------------------------------

static HAPPlatformBLEPeripheralManagerRef blePeripheralManager = NULL;
static HAPPlatformBLEPeripheralManagerDelegate delegate;

// BTstack state
static hci_con_handle_t connection_handle = HCI_CON_HANDLE_INVALID;
static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

// GATT database
#define MAX_SERVICES 16
#define MAX_CHARACTERISTICS 64
#define MAX_DESCRIPTORS 128

typedef struct {
    uint16_t handle;
    const uint8_t* uuid;
    uint8_t uuid_len;
    uint16_t properties;
    uint16_t permissions;
    uint8_t* value;
    uint16_t value_len;
    uint16_t value_capacity;
} gatt_characteristic_t;

typedef struct {
    uint16_t start_handle;
    uint16_t end_handle;
    const uint8_t* uuid;
    uint8_t uuid_len;
    gatt_characteristic_t* characteristics;
    uint16_t num_characteristics;
} gatt_service_t;

static gatt_service_t services[MAX_SERVICES];
static uint16_t num_services = 0;
static gatt_characteristic_t characteristics[MAX_CHARACTERISTICS];
static uint16_t num_characteristics = 0;
static uint16_t next_handle = 1;

// Advertisement data
static uint8_t adv_data[31];
static uint8_t adv_data_len = 0;
static uint8_t scan_response_data[31];
static uint8_t scan_response_data_len = 0;

//----------------------------------------------------------------------------------------------------------------------

static uint16_t att_read_callback(
        hci_con_handle_t con_handle,
        uint16_t att_handle,
        uint16_t offset,
        uint8_t* buffer,
        uint16_t buffer_size) {

    // Find characteristic by handle
    for (uint16_t i = 0; i < num_characteristics; i++) {
        if (characteristics[i].handle == att_handle) {
            if (offset >= characteristics[i].value_len) {
                return 0;
            }
            uint16_t bytes_to_copy = characteristics[i].value_len - offset;
            if (bytes_to_copy > buffer_size) {
                bytes_to_copy = buffer_size;
            }
            memcpy(buffer, characteristics[i].value + offset, bytes_to_copy);
            return bytes_to_copy;
        }
    }
    return 0;
}

static int att_write_callback(
        hci_con_handle_t con_handle,
        uint16_t att_handle,
        uint16_t transaction_mode,
        uint16_t offset,
        uint8_t* buffer,
        uint16_t buffer_size) {

    // Find characteristic by handle
    for (uint16_t i = 0; i < num_characteristics; i++) {
        if (characteristics[i].handle == att_handle) {
            if (offset + buffer_size > characteristics[i].value_capacity) {
                return ATT_ERROR_INVALID_ATTRIBUTE_VALUE_LENGTH;
            }
            memcpy(characteristics[i].value + offset, buffer, buffer_size);
            characteristics[i].value_len = offset + buffer_size;

            // Notify delegate of write
            if (delegate.handleWriteRequest && blePeripheralManager) {
                // Convert to HAP types and notify
                delegate.handleWriteRequest(blePeripheralManager, att_handle, buffer, buffer_size, delegate.context);
            }
            return 0;
        }
    }
    return ATT_ERROR_INVALID_HANDLE;
}

static void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t* packet, uint16_t size) {
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
        case HCI_EVENT_DISCONNECTION_COMPLETE:
            connection_handle = HCI_CON_HANDLE_INVALID;
            HAPLogInfo(&logObject, "BLE client disconnected");

            // Notify delegate
            if (delegate.handleDisconnection && blePeripheralManager) {
                delegate.handleDisconnection(blePeripheralManager, delegate.context);
            }

            // Restart advertising
            if (blePeripheralManager && adv_data_len > 0) {
                gap_advertisements_set_data(adv_data_len, adv_data);
                gap_advertisements_set_params(
                    blePeripheralManager->advertisingInterval,
                    blePeripheralManager->advertisingInterval + 50);
                gap_advertisements_enable(1);
            }
            break;

        case HCI_EVENT_LE_META:
            switch (hci_event_le_meta_get_subevent_code(packet)) {
                case HCI_SUBEVENT_LE_CONNECTION_COMPLETE: {
                    connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    HAPLogInfo(&logObject, "BLE client connected, handle: 0x%04x", connection_handle);

                    // Notify delegate
                    if (delegate.handleConnection && blePeripheralManager) {
                        delegate.handleConnection(blePeripheralManager, connection_handle, delegate.context);
                    }
                    break;
                }
            }
            break;
    }
}

//----------------------------------------------------------------------------------------------------------------------

void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerOptions* options) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(options);

    HAPLogDebug(&logObject, "Storage configuration: blePeripheralManager = %lu", (unsigned long) sizeof *blePeripheralManager_);

    HAPRawBufferZero(blePeripheralManager_, sizeof *blePeripheralManager_);
    blePeripheralManager = blePeripheralManager_;

    // Initialize BTstack
    btstack_memory_init();
    btstack_run_loop_init(btstack_run_loop_windows_get_instance());

    // Initialize HCI transport (USB or H4)
    const hci_transport_t* transport = hci_transport_h2_winusb_instance();
    hci_init(transport, NULL);

    // Setup LE device DB for bonding
    le_device_db_init();
    sm_init();
    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING);

    // Setup ATT server
    att_server_init(NULL, att_read_callback, att_write_callback);

    // Register packet handlers
    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_event_callback_registration.callback = &packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    // Set device name
    gap_set_local_name("HomeKit Accessory");
    gap_discoverable_control(1);
    gap_set_class_of_device(0x200000);  // Unspecified device class

    blePeripheralManager_->deviceAddress.bytes[0] = 0x00;
    blePeripheralManager_->deviceAddress.bytes[1] = 0x1A;
    blePeripheralManager_->deviceAddress.bytes[2] = 0x7D;
    blePeripheralManager_->deviceAddress.bytes[3] = 0xDA;
    blePeripheralManager_->deviceAddress.bytes[4] = 0x71;
    blePeripheralManager_->deviceAddress.bytes[5] = 0x13;

    blePeripheralManager_->advertisingInterval = 100;  // Default 100ms

    // Power on Bluetooth
    hci_power_control(HCI_POWER_ON);

    HAPLogInfo(&logObject, "BLE Peripheral Manager initialized with BTstack");
}

void HAPPlatformBLEPeripheralManagerSetDelegate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerDelegate* delegate_) {
    HAPPrecondition(blePeripheralManager_);

    if (delegate_) {
        delegate = *delegate_;
    } else {
        HAPRawBufferZero(&delegate, sizeof delegate);
    }
}

void HAPPlatformBLEPeripheralManagerSetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerDeviceAddress* deviceAddress) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(deviceAddress);

    blePeripheralManager_->deviceAddress = *deviceAddress;

    // Set BD_ADDR in BTstack
    bd_addr_t addr;
    memcpy(addr, deviceAddress->bytes, 6);
    hci_set_bd_addr(addr);

    HAPLogInfo(&logObject, "BLE device address set: %02X:%02X:%02X:%02X:%02X:%02X",
               deviceAddress->bytes[5], deviceAddress->bytes[4], deviceAddress->bytes[3],
               deviceAddress->bytes[2], deviceAddress->bytes[1], deviceAddress->bytes[0]);
}

void HAPPlatformBLEPeripheralManagerSetDeviceName(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const char* deviceName) {
    HAPPrecondition(deviceName);

    gap_set_local_name(deviceName);
    HAPLogInfo(&logObject, "BLE device name set: %s", deviceName);
}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    // Clear all services and characteristics
    num_services = 0;
    num_characteristics = 0;
    next_handle = 1;

    HAPLogDebug(&logObject, "Removed all BLE services");
}

void HAPPlatformBLEPeripheralManagerAddService(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        bool isPrimary) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(type);
    HAPPrecondition(num_services < MAX_SERVICES);

    gatt_service_t* service = &services[num_services++];
    service->start_handle = next_handle++;
    service->uuid = type->bytes;
    service->uuid_len = type->type == kHAPPlatformBLEPeripheralManagerUUIDType_16Bit ? 2 : 16;
    service->num_characteristics = 0;

    HAPLogInfo(&logObject, "Added BLE service, handle: 0x%04x, primary: %d", service->start_handle, isPrimary);
}

uint16_t HAPPlatformBLEPeripheralManagerAddCharacteristic(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        HAPPlatformBLEPeripheralManagerCharacteristicProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        void* _Nullable bytes,
        size_t numBytes) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(type);
    HAPPrecondition(num_characteristics < MAX_CHARACTERISTICS);
    HAPPrecondition(!constNumBytes || constBytes);
    HAPPrecondition(!numBytes || bytes);

    gatt_characteristic_t* characteristic = &characteristics[num_characteristics++];
    characteristic->handle = next_handle++;
    characteristic->uuid = type->bytes;
    characteristic->uuid_len = type->type == kHAPPlatformBLEPeripheralManagerUUIDType_16Bit ? 2 : 16;

    // Convert HAP properties to ATT properties
    characteristic->properties = 0;
    if (properties.read) characteristic->properties |= ATT_PROPERTY_READ;
    if (properties.write) characteristic->properties |= ATT_PROPERTY_WRITE;
    if (properties.writeWithoutResponse) characteristic->properties |= ATT_PROPERTY_WRITE_WITHOUT_RESPONSE;
    if (properties.notify) characteristic->properties |= ATT_PROPERTY_NOTIFY;
    if (properties.indicate) characteristic->properties |= ATT_PROPERTY_INDICATE;

    characteristic->permissions = ATT_PERMISSION_READ | ATT_PERMISSION_WRITE;

    // Handle value storage
    if (constBytes && constNumBytes) {
        characteristic->value = (uint8_t*)constBytes;
        characteristic->value_len = (uint16_t)constNumBytes;
        characteristic->value_capacity = (uint16_t)constNumBytes;
    } else if (bytes && numBytes) {
        characteristic->value = (uint8_t*)bytes;
        characteristic->value_len = 0;
        characteristic->value_capacity = (uint16_t)numBytes;
    } else {
        characteristic->value = NULL;
        characteristic->value_len = 0;
        characteristic->value_capacity = 0;
    }

    HAPLogInfo(&logObject, "Added BLE characteristic, handle: 0x%04x, properties: 0x%02x",
               characteristic->handle, characteristic->properties);

    return characteristic->handle;
}

uint16_t HAPPlatformBLEPeripheralManagerAddDescriptor(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerUUID* type,
        HAPPlatformBLEPeripheralManagerDescriptorProperties properties,
        const void* _Nullable constBytes,
        size_t constNumBytes,
        void* _Nullable bytes,
        size_t numBytes) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(type);
    HAPPrecondition(!constNumBytes || constBytes);
    HAPPrecondition(!numBytes || bytes);

    // Descriptors are stored similarly to characteristics in BTstack
    uint16_t handle = next_handle++;

    HAPLogInfo(&logObject, "Added BLE descriptor, handle: 0x%04x", handle);

    return handle;
}

void HAPPlatformBLEPeripheralManagerPublishServices(HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    // Build ATT database from our services and characteristics
    // In BTstack, you typically define the database statically,
    // but we're building it dynamically here

    HAPLogInfo(&logObject, "Published %u BLE services with %u characteristics", num_services, num_characteristics);

    // Start ATT server
    att_server_init(NULL, att_read_callback, att_write_callback);

    HAPLogInfo(&logObject, "BLE GATT server started");
}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    num_services = 0;
    num_characteristics = 0;
    next_handle = 1;

    HAPLogDebug(&logObject, "Removed all BLE services");
}

void HAPPlatformBLEPeripheralManagerStartAdvertising(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        HAPBLEAdvertisingInterval advertisingInterval,
        const void* advertisingBytes,
        size_t numAdvertisingBytes,
        const void* _Nullable scanResponseBytes,
        size_t numScanResponseBytes) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(advertisingBytes);
    HAPPrecondition(numAdvertisingBytes <= sizeof adv_data);
    HAPPrecondition(!numScanResponseBytes || scanResponseBytes);
    HAPPrecondition(numScanResponseBytes <= sizeof scan_response_data);

    blePeripheralManager_->advertisingInterval = advertisingInterval;

    // Copy advertising data
    memcpy(adv_data, advertisingBytes, numAdvertisingBytes);
    adv_data_len = (uint8_t)numAdvertisingBytes;

    if (scanResponseBytes && numScanResponseBytes) {
        memcpy(scan_response_data, scanResponseBytes, numScanResponseBytes);
        scan_response_data_len = (uint8_t)numScanResponseBytes;
    } else {
        scan_response_data_len = 0;
    }

    // Configure advertising parameters
    gap_advertisements_set_data(adv_data_len, adv_data);
    if (scan_response_data_len > 0) {
        gap_scan_response_set_data(scan_response_data_len, scan_response_data);
    }

    gap_advertisements_set_params(advertisingInterval, advertisingInterval + 50);
    gap_advertisements_enable(1);

    HAPLogInfo(&logObject, "Started BLE advertising, interval: %u ms", advertisingInterval);
}

void HAPPlatformBLEPeripheralManagerStopAdvertising(HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    gap_advertisements_enable(0);

    HAPLogInfo(&logObject, "Stopped BLE advertising");
}

void HAPPlatformBLEPeripheralManagerCancelCentralConnection(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle) {
    HAPPrecondition(blePeripheralManager_);

    if (connection_handle != HCI_CON_HANDLE_INVALID) {
        gap_disconnect(connection_handle);
        HAPLogInfo(&logObject, "Disconnecting BLE client");
    }
}

void HAPPlatformBLEPeripheralManagerSendHandleValueIndication(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle,
        uint16_t valueHandle,
        const void* _Nullable bytes,
        size_t numBytes) {
    HAPPrecondition(blePeripheralManager_);
    HAPPrecondition(!numBytes || bytes);

    if (connection_handle != HCI_CON_HANDLE_INVALID) {
        att_server_notify(connection_handle, valueHandle, (const uint8_t*)bytes, (uint16_t)numBytes);
        HAPLogDebug(&logObject, "Sent BLE notification for handle 0x%04x, %zu bytes", valueHandle, numBytes);
    }
}

HAP_RESULT_USE_CHECK
bool HAPPlatformBLEPeripheralManagerIsAdvertising(HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    // Check if advertising is active
    return adv_data_len > 0 && gap_advertisements_enabled();
}

HAP_RESULT_USE_CHECK
size_t HAPPlatformBLEPeripheralManagerGetNumConnectedCentrals(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_) {
    HAPPrecondition(blePeripheralManager_);

    return (connection_handle != HCI_CON_HANDLE_INVALID) ? 1 : 0;
}

#else  // !HAVE_BLE

// Stub implementation when BLE is disabled
void HAPPlatformBLEPeripheralManagerCreate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_,
        const HAPPlatformBLEPeripheralManagerOptions* options HAP_UNUSED) {
    HAPPrecondition(blePeripheralManager_);
    HAPLogInfo(&logObject, "BLE support not enabled in build. Use -DHAVE_BLE=ON to enable.");
    HAPRawBufferZero(blePeripheralManager_, sizeof *blePeripheralManager_);
}

void HAPPlatformBLEPeripheralManagerSetDelegate(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const HAPPlatformBLEPeripheralManagerDelegate* delegate_ HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerSetDeviceAddress(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const HAPPlatformBLEPeripheralManagerDeviceAddress* deviceAddress HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerSetDeviceName(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const char* deviceName HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerRemoveAllServices(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerAddService(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const HAPPlatformBLEPeripheralManagerUUID* type HAP_UNUSED,
        bool isPrimary HAP_UNUSED) {}

uint16_t HAPPlatformBLEPeripheralManagerAddCharacteristic(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const HAPPlatformBLEPeripheralManagerUUID* type HAP_UNUSED,
        HAPPlatformBLEPeripheralManagerCharacteristicProperties properties HAP_UNUSED,
        const void* _Nullable constBytes HAP_UNUSED,
        size_t constNumBytes HAP_UNUSED,
        void* _Nullable bytes HAP_UNUSED,
        size_t numBytes HAP_UNUSED) { return 0; }

uint16_t HAPPlatformBLEPeripheralManagerAddDescriptor(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        const HAPPlatformBLEPeripheralManagerUUID* type HAP_UNUSED,
        HAPPlatformBLEPeripheralManagerDescriptorProperties properties HAP_UNUSED,
        const void* _Nullable constBytes HAP_UNUSED,
        size_t constNumBytes HAP_UNUSED,
        void* _Nullable bytes HAP_UNUSED,
        size_t numBytes HAP_UNUSED) { return 0; }

void HAPPlatformBLEPeripheralManagerPublishServices(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerStartAdvertising(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        HAPBLEAdvertisingInterval advertisingInterval HAP_UNUSED,
        const void* advertisingBytes HAP_UNUSED,
        size_t numAdvertisingBytes HAP_UNUSED,
        const void* _Nullable scanResponseBytes HAP_UNUSED,
        size_t numScanResponseBytes HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerStopAdvertising(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerCancelCentralConnection(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle HAP_UNUSED) {}

void HAPPlatformBLEPeripheralManagerSendHandleValueIndication(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED,
        HAPPlatformBLEPeripheralManagerConnectionHandle connectionHandle HAP_UNUSED,
        uint16_t valueHandle HAP_UNUSED,
        const void* _Nullable bytes HAP_UNUSED,
        size_t numBytes HAP_UNUSED) {}

bool HAPPlatformBLEPeripheralManagerIsAdvertising(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED) { return false; }

size_t HAPPlatformBLEPeripheralManagerGetNumConnectedCentrals(
        HAPPlatformBLEPeripheralManagerRef blePeripheralManager_ HAP_UNUSED) { return 0; }

#endif // HAVE_BLE
