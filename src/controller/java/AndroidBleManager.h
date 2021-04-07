/*
 *   Copyright (c) 2021 Project CHIP Authors
 *   All rights reserved.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */
#pragma once

#include <stack/ControllerStackImpl.h>

#include "AndroidBleApplicationDelegate.h"
#include <controller/java/AndroidBlePlatformDelegate.h>
#include <controller/java/AndroidBleConnectionDelegate.h>

#if CONFIG_NETWORK_LAYER_BLE
class AndroidBleManager : chip::BleLayerConfiguration {
public:
    CHIP_ERROR Init(chip::ControllerStackImpl<AndroidBleManager>* stack);

    chip::Ble::BleLayer * Get() { return &sBleLayer; }
private:
    chip::Ble::BleLayer sBleLayer;
    AndroidBleApplicationDelegate sBleApplicationDelegate;
    AndroidBlePlatformDelegate sBlePlatformDelegate;
    AndroidBleConnectionDelegate sBleConnectionDelegate;

    static void HandleNotifyChipConnectionClosed(BLE_CONNECTION_OBJECT connObj);
    static bool HandleSendCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId,
        const uint8_t * characteristicData, uint32_t characteristicDataLen);
    static bool HandleSubscribeCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId);
    static bool HandleUnsubscribeCharacteristic(BLE_CONNECTION_OBJECT connObj, const uint8_t * svcId, const uint8_t * charId);
    static bool HandleCloseConnection(BLE_CONNECTION_OBJECT connObj);
    static uint16_t HandleGetMTU(BLE_CONNECTION_OBJECT connObj);
    static void HandleNewConnection(void * appState, const uint16_t discriminator);
};
#endif
